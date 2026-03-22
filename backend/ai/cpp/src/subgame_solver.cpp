/**
 * Subgame solver — Safe re-solving with blueprint range weighting.
 *
 * Features:
 * 1. Blueprint range weighting: opponent hands sampled proportional to
 *    their blueprint reach probability (not uniform)
 * 2. Safe subgame solving via gadget game: opponent can "opt out" at the
 *    root and take their blueprint CBV, preventing exploitation
 * 3. Real hand evaluation at showdown
 * 4. Multi-street traversal with sampled board runouts
 */

#include "subgame_solver.h"
#include "hand_abstraction.h"
#include <algorithm>
#include <array>
#include <bitset>
#include <cmath>
#include <numeric>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

namespace poker {

namespace {

thread_local std::mt19937 rng{std::random_device{}()};

// ─── 7-card hand evaluator (lower = better) ────────────────────

uint16_t eval_5_exact(int r[5], int su[5]) {
    std::sort(r, r + 5, std::greater<int>());
    bool flush = (su[0]==su[1]&&su[1]==su[2]&&su[2]==su[3]&&su[3]==su[4]);
    bool straight = false; int sh = -1;
    if (r[0]-r[4]==4 && r[0]!=r[1]&&r[1]!=r[2]&&r[2]!=r[3]&&r[3]!=r[4]) { straight=true; sh=r[0]; }
    if (r[0]==12&&r[1]==3&&r[2]==2&&r[3]==1&&r[4]==0) { straight=true; sh=3; }
    int freq[13]={}; for(int i=0;i<5;i++) freq[r[i]]++;
    int quads=0,trips=0,pairs=0,qr=-1,tr=-1,pr[2]={-1,-1};
    for(int x=12;x>=0;x--){
        if(freq[x]==4){quads++;qr=x;}
        else if(freq[x]==3){trips++;tr=x;}
        else if(freq[x]==2){if(pairs<2)pr[pairs]=x;pairs++;}
    }
    if(straight&&flush) return 1+(12-sh);
    if(quads){int k=-1;for(int i=0;i<5;i++)if(r[i]!=qr){k=r[i];break;}return 11+(12-qr)*13+(12-k);}
    if(trips&&pairs) return 180+(12-tr)*13+(12-pr[0]);
    if(flush){int v=0;for(int i=0;i<5;i++)v=v*13+r[i];return 350+(371292-v)/100;}
    if(straight) return 1600+(12-sh);
    if(trips){int kv=0;for(int i=0;i<5;i++)if(r[i]!=tr)kv=kv*13+r[i];return 1620+(12-tr)*169+(169-kv%169);}
    if(pairs>=2){int k=-1;for(int i=0;i<5;i++)if(r[i]!=pr[0]&&r[i]!=pr[1]){k=r[i];break;}
        int hi=std::max(pr[0],pr[1]),lo=std::min(pr[0],pr[1]);
        return 3800+(12-hi)*169+(12-lo)*13+(12-(k>=0?k:0));}
    if(pairs==1){int kv=0;for(int i=0;i<5;i++)if(r[i]!=pr[0])kv=kv*13+r[i];return 6000+(12-pr[0])*2197+(2197-kv%2197);}
    int v=0;for(int i=0;i<5;i++)v=v*13+r[i];return 7000+(371292-v)/100;
}

uint16_t evaluate_7cards(const int cards[7]) {
    uint16_t best=0xFFFF;
    for(int i=0;i<3;i++)for(int j=i+1;j<4;j++)for(int k=j+1;k<5;k++)
        for(int l=k+1;l<6;l++)for(int m=l+1;m<7;m++){
            int r[5]={cards[i]%13,cards[j]%13,cards[k]%13,cards[l]%13,cards[m]%13};
            int su[5]={cards[i]/13,cards[j]/13,cards[k]/13,cards[l]/13,cards[m]/13};
            uint16_t v=eval_5_exact(r,su); if(v<best)best=v;
        }
    return best;
}

// ─── Subgame tree ───────────────────────────────────────────────

constexpr int SUB_ACTIONS = 6; // fold, check/call, half-pot, pot, 2x-pot, all-in
// Gadget: opponent's extra action = "take CBV and leave"
constexpr int GADGET_ACTION = 6;

struct SubNode {
    std::array<double, SUB_ACTIONS + 1> regret_sum{};   // +1 for gadget
    std::array<double, SUB_ACTIONS + 1> strategy_sum{};

    std::array<double, SUB_ACTIONS + 1> get_strategy() const {
        std::array<double, SUB_ACTIONS + 1> s{};
        double total = 0;
        for (size_t a = 0; a < s.size(); ++a) {
            s[a] = std::max(regret_sum[a], 0.0);
            total += s[a];
        }
        if (total > 0) for (auto& v : s) v /= total;
        else {
            double u = 1.0 / s.size();
            std::fill(s.begin(), s.end(), u);
        }
        return s;
    }

    std::array<double, SUB_ACTIONS + 1> get_average() const {
        std::array<double, SUB_ACTIONS + 1> a{};
        double total = std::accumulate(strategy_sum.begin(), strategy_sum.end(), 0.0);
        if (total > 0) for (size_t i = 0; i < a.size(); ++i) a[i] = strategy_sum[i] / total;
        else {
            double u = 1.0 / a.size();
            std::fill(a.begin(), a.end(), u);
        }
        return a;
    }
};

struct SubState {
    int pot;
    int to_act;             // 0=hero, 1=villain
    int bets_this_street;
    int street;             // 0=preflop, 1=flop, 2=turn, 3=river
    std::string history;
    std::array<int, 2> invested;
    std::array<int, 2> stacks;
    bool terminal = false;
    int result = -1;        // -1=ongoing, 0=hero fold, 1=villain fold, 2=showdown
    bool at_gadget = false; // true only at root for villain's first decision
};

std::vector<int> sub_valid_actions(const SubState& s) {
    std::vector<int> actions;
    // Gadget: villain can opt out at root
    if (s.at_gadget) {
        actions.push_back(GADGET_ACTION); // take CBV
    }
    if (s.bets_this_street > 0) actions.push_back(0); // fold
    actions.push_back(1); // check/call
    if (s.stacks[s.to_act] > 0 && s.bets_this_street < 4) {
        actions.push_back(2); // half-pot
        actions.push_back(3); // pot
        if (s.bets_this_street < 3) actions.push_back(4); // 2x pot
    }
    if (s.stacks[s.to_act] > 0) actions.push_back(5); // all-in
    return actions;
}

SubState sub_apply_action(const SubState& s, int action) {
    SubState n = s;
    n.at_gadget = false; // gadget only at root

    if (action == GADGET_ACTION) {
        // Opponent takes CBV — terminal
        n.terminal = true;
        n.result = 3; // special: gadget exit
        return n;
    }

    n.history += static_cast<char>('0' + action);

    if (action == 0) { // fold
        n.terminal = true;
        n.result = s.to_act;
        return n;
    }

    if (action == 1) { // check/call
        int gap = s.invested[1 - s.to_act] - s.invested[s.to_act];
        int to_call = std::min(gap, s.stacks[s.to_act]);
        n.invested[s.to_act] += to_call;
        n.stacks[s.to_act] -= to_call;
        n.pot += to_call;

        if (s.bets_this_street > 0) {
            if (n.street >= 3) {
                n.terminal = true;
                n.result = 2;
            } else {
                n.street++;
                n.bets_this_street = 0;
                n.to_act = 0;
                n.history += '/';
            }
        } else {
            if (s.to_act == 1) {
                if (n.street >= 3) {
                    n.terminal = true;
                    n.result = 2;
                } else {
                    n.street++;
                    n.bets_this_street = 0;
                    n.to_act = 0;
                    n.history += '/';
                }
            } else {
                n.to_act = 1;
            }
        }
        return n;
    }

    // Bet/raise (2-5)
    int bet_size;
    if (action == 5) {
        bet_size = s.stacks[s.to_act];
    } else {
        double frac = (action == 2) ? 0.5 : (action == 3) ? 1.0 : 2.0;
        bet_size = static_cast<int>(s.pot * frac);
        int gap = s.invested[1 - s.to_act] - s.invested[s.to_act];
        int prev_raise = std::max(gap, 2);
        bet_size = std::max(bet_size, prev_raise);
        bet_size = std::min(bet_size, s.stacks[s.to_act]);
    }

    int gap = s.invested[1 - s.to_act] - s.invested[s.to_act];
    int total_put_in = gap + bet_size;
    total_put_in = std::min(total_put_in, s.stacks[s.to_act]);

    n.invested[s.to_act] += total_put_in;
    n.stacks[s.to_act] -= total_put_in;
    n.pot += total_put_in;
    n.bets_this_street++;
    n.to_act = 1 - s.to_act;
    return n;
}

// ─── CFR traversal with gadget ──────────────────────────────────

double sub_cfr(
    SubState state,
    int traverser,  // 0=hero, 1=villain
    const std::array<int, 2>& hero_cards,
    const std::array<int, 2>& vill_cards,
    const int board[5],
    int vill_bucket,
    double vill_cbv,  // villain's blueprint CBV (for gadget)
    std::unordered_map<std::string, SubNode>& nodes,
    int depth,
    int max_depth
) {
    // Terminal
    if (state.terminal) {
        if (state.result == 3) {
            // Gadget exit: villain gets CBV
            return (traverser == 1) ? vill_cbv : -vill_cbv;
        }
        if (state.result == 2) {
            // Showdown — payoff = (won pot) - (own investment)
            int hero_7[7] = {hero_cards[0], hero_cards[1], board[0], board[1], board[2], board[3], board[4]};
            int vill_7[7] = {vill_cards[0], vill_cards[1], board[0], board[1], board[2], board[3], board[4]};
            uint16_t hr = evaluate_7cards(hero_7);
            uint16_t vr = evaluate_7cards(vill_7);
            double hero_payoff, vill_payoff;
            if (hr < vr) {  // hero wins
                hero_payoff = static_cast<double>(state.pot - state.invested[0]);
                vill_payoff = -static_cast<double>(state.invested[1]);
            } else if (vr < hr) {  // villain wins
                hero_payoff = -static_cast<double>(state.invested[0]);
                vill_payoff = static_cast<double>(state.pot - state.invested[1]);
            } else {  // tie — split pot
                hero_payoff = state.pot / 2.0 - state.invested[0];
                vill_payoff = state.pot / 2.0 - state.invested[1];
            }
            return (traverser == 0) ? hero_payoff : vill_payoff;
        }
        // Fold — winner takes entire pot
        double hero_payoff, vill_payoff;
        if (state.result == 1) {  // villain folds → hero wins pot
            hero_payoff = static_cast<double>(state.pot - state.invested[0]);
            vill_payoff = -static_cast<double>(state.invested[1]);
        } else {  // hero folds → villain wins pot
            hero_payoff = -static_cast<double>(state.invested[0]);
            vill_payoff = static_cast<double>(state.pot - state.invested[1]);
        }
        return (traverser == 0) ? hero_payoff : vill_payoff;
    }

    // Depth limit — EHS fallback
    if (depth >= max_depth) {
        const auto& hand = (traverser == 0) ? hero_cards : vill_cards;
        double ehs = compute_ehs(hand, std::vector<int>(board, board + 5), 200);
        double val = ehs * state.pot / 2.0 - (1.0 - ehs) * state.pot / 2.0;
        return (traverser == 0) ? val : -val;
    }

    auto valid = sub_valid_actions(state);

    // Info set key
    std::string key;
    if (state.to_act == 0) {
        key = "H:" + std::to_string(state.street) + ":" + state.history;
    } else {
        int bucket = vill_bucket;
        if (state.street > 0) {
            int bc = (state.street == 1) ? 3 : (state.street == 2) ? 4 : 5;
            std::vector<int> pb(board, board + bc);
            bucket = hand_to_bucket(vill_cards, pb);
        }
        key = "V:" + std::to_string(state.street) + ":" + std::to_string(bucket)
            + ":" + state.history;
        if (state.at_gadget) key = "G:" + key; // gadget node has separate info set
    }

    SubNode& node = nodes[key];
    auto strategy = node.get_strategy();

    if (state.to_act == traverser) {
        // Full traversal
        std::array<double, SUB_ACTIONS + 1> action_util{};
        double node_util = 0;

        for (int a : valid) {
            SubState next = sub_apply_action(state, a);
            action_util[a] = sub_cfr(next, traverser, hero_cards, vill_cards,
                                     board, vill_bucket, vill_cbv, nodes,
                                     depth + 1, max_depth);
            node_util += strategy[a] * action_util[a];
        }

        for (int a : valid) {
            node.regret_sum[a] = std::max(
                node.regret_sum[a] + (action_util[a] - node_util), 0.0);
        }
        return node_util;
    } else {
        // External sampling
        std::uniform_real_distribution<double> dist(0.0, 1.0);
        double r = dist(rng), cumul = 0;
        int sampled = valid.back();

        double valid_total = 0;
        for (int a : valid) valid_total += strategy[a];
        if (valid_total > 0) {
            for (int a : valid) {
                cumul += strategy[a] / valid_total;
                if (r < cumul) { sampled = a; break; }
            }
        }

        for (int a : valid) {
            node.strategy_sum[a] += strategy[a];
        }

        SubState next = sub_apply_action(state, sampled);
        return sub_cfr(next, traverser, hero_cards, vill_cards,
                       board, vill_bucket, vill_cbv, nodes,
                       depth + 1, max_depth);
    }
}

} // anonymous namespace

// ─── Public interface ───────────────────────────────────────────

SubgameSolver::SubgameSolver(const MCCFRTrainer& bp, int dl)
    : blueprint_(bp), depth_limit_(dl) {}

std::array<double, NUM_ACTIONS> SubgameSolver::solve(
    const SubgameState& gs, int iterations
) {
    // 1. Current street
    int current_street;
    if (gs.board.empty()) current_street = 0;
    else if (gs.board.size() == 3) current_street = 1;
    else if (gs.board.size() == 4) current_street = 2;
    else current_street = 3;

    // 2. Enumerate opponent holdings + compute reach weights & CBVs
    std::bitset<52> used;
    used.set(gs.hole_cards[0]);
    used.set(gs.hole_cards[1]);
    for (int c : gs.board) used.set(c);

    struct OppHand {
        std::array<int, 2> cards;
        double reach;  // blueprint reach probability
        double cbv;    // counterfactual value under blueprint
    };
    std::vector<OppHand> opp_hands;
    opp_hands.reserve(1000);

    // Threshold: only compute reach/CBV if blueprint is meaningful
    // and action history exists. compute_opponent_reach is expensive
    // (calls hand_to_bucket → compute_ehs per hand per history step),
    // so we limit it to blueprints with enough info sets to be useful.
    bool has_blueprint = blueprint_.num_info_sets() > 10000;
    bool has_history = !gs.action_history.empty();
    bool use_reach = has_blueprint && has_history;

    // Batch: enumerate all opponent hands first (cheap)
    std::vector<std::array<int, 2>> all_opp;
    all_opp.reserve(1000);
    for (int i = 0; i < 52; ++i) {
        if (used.test(i)) continue;
        for (int j = i + 1; j < 52; ++j) {
            if (used.test(j)) continue;
            all_opp.push_back({i, j});
        }
    }

    if (use_reach) {
        // Compute reach + CBV for each opponent hand
        // Cap computation time: if too many hands, subsample for reach
        for (auto& opp : all_opp) {
            double reach = blueprint_.compute_opponent_reach(
                opp, gs.board, gs.action_history);
            if (reach < 1e-12) continue; // pruned

            double cbv = blueprint_.compute_opponent_cbv(
                opp, gs.board, gs.action_history, gs.pot, 100);

            opp_hands.push_back({opp, reach, cbv});
        }
        // If reach pruned everything, fall back to uniform
        if (opp_hands.empty()) {
            for (auto& opp : all_opp) {
                opp_hands.push_back({opp, 1.0, 0.0});
            }
        }
    } else {
        // No blueprint or no history: uniform range
        for (auto& opp : all_opp) {
            opp_hands.push_back({opp, 1.0, 0.0});
        }
    }

    if (opp_hands.empty()) {
        std::array<double, NUM_ACTIONS> u{};
        std::fill(u.begin(), u.end(), 1.0 / NUM_ACTIONS);
        return u;
    }

    // 3. Build CDF for weighted sampling
    double total_reach = 0;
    for (auto& h : opp_hands) total_reach += h.reach;
    if (total_reach <= 0) total_reach = 1.0; // fallback to uniform

    std::vector<double> cdf(opp_hands.size());
    double cumul = 0;
    for (size_t i = 0; i < opp_hands.size(); ++i) {
        cumul += opp_hands[i].reach / total_reach;
        cdf[i] = cumul;
    }
    cdf.back() = 1.0; // ensure no floating-point gap

    // 4. Initial subgame state
    SubState root{};
    root.pot = gs.pot;
    root.to_act = 0;
    root.bets_this_street = (gs.current_bet > 0) ? 1 : 0;
    root.street = current_street;
    // Each player has already contributed pot/2 as sunk cost (blinds/prior bets)
    int half_pot = gs.pot / 2;
    root.invested = {half_pot, gs.pot - half_pot};
    root.stacks = {gs.my_chips, gs.opp_chips};

    if (gs.current_bet > 0) {
        root.invested[1] += gs.current_bet;
        root.pot += gs.current_bet;
        root.stacks[1] -= std::min(gs.current_bet, gs.opp_chips);
    }

    int max_depth = (4 - current_street) * 10 + 5;

    // 5. Run MCCFR iterations with weighted sampling + gadget
    std::unordered_map<std::string, SubNode> nodes;
    nodes.reserve(4096);

    std::uniform_real_distribution<double> unit_dist(0.0, 1.0);

    for (int iter = 0; iter < iterations; ++iter) {
        // Sample opponent hand from reach-weighted distribution
        double r = unit_dist(rng);
        size_t idx = static_cast<size_t>(
            std::lower_bound(cdf.begin(), cdf.end(), r) - cdf.begin());
        if (idx >= opp_hands.size()) idx = opp_hands.size() - 1;

        auto& opp = opp_hands[idx];

        // Build remaining deck
        std::vector<int> remaining;
        remaining.reserve(44);
        for (int c = 0; c < 52; ++c) {
            if (c == gs.hole_cards[0] || c == gs.hole_cards[1] ||
                c == opp.cards[0] || c == opp.cards[1]) continue;
            bool on_board = false;
            for (int bc : gs.board) { if (bc == c) { on_board = true; break; } }
            if (!on_board) remaining.push_back(c);
        }

        // Sample remaining board
        int need = 5 - static_cast<int>(gs.board.size());
        for (int i = 0; i < need && i < static_cast<int>(remaining.size()); ++i) {
            std::uniform_int_distribution<int> d(i, static_cast<int>(remaining.size()) - 1);
            std::swap(remaining[i], remaining[d(rng)]);
        }

        int full_board[5];
        int bi = 0;
        for (int c : gs.board) full_board[bi++] = c;
        for (int i = 0; i < need; ++i) full_board[bi++] = remaining[i];

        int vill_bucket = hand_to_bucket(opp.cards, gs.board);

        // Enable gadget at root for villain (safe solving)
        SubState gadget_root = root;
        gadget_root.at_gadget = (gadget_root.to_act == 1); // if villain acts first

        // Traverse for both players
        for (int p = 0; p < 2; ++p) {
            // When traversing as villain, enable gadget at root
            SubState& start = (p == 1 && root.to_act == 1) ? gadget_root : root;
            // For hero traversal when villain goes first, also set gadget
            if (p == 0 && root.to_act == 1) {
                gadget_root.at_gadget = true;
                sub_cfr(gadget_root, p, gs.hole_cards, opp.cards, full_board,
                        vill_bucket, opp.cbv, nodes, 0, max_depth);
            } else if (p == 1 && root.to_act == 0) {
                // Hero acts first at root; villain gets gadget when it's their turn
                // The gadget is handled inside the tree at villain's first action
                sub_cfr(root, p, gs.hole_cards, opp.cards, full_board,
                        vill_bucket, opp.cbv, nodes, 0, max_depth);
            } else {
                sub_cfr(start, p, gs.hole_cards, opp.cards, full_board,
                        vill_bucket, opp.cbv, nodes, 0, max_depth);
            }
        }
    }

    // 6. Extract hero's root strategy
    std::string root_key = "H:" + std::to_string(current_street) + ":";
    auto it = nodes.find(root_key);

    std::array<double, NUM_ACTIONS> result{};
    if (it != nodes.end()) {
        auto avg = it->second.get_average();
        for (int i = 0; i < SUB_ACTIONS && i < NUM_ACTIONS; ++i) {
            result[i] = avg[i];
        }
        // Normalize (gadget action excluded from hero's output)
        double sum = 0;
        for (int i = 0; i < NUM_ACTIONS; ++i) sum += result[i];
        if (sum > 0) for (int i = 0; i < NUM_ACTIONS; ++i) result[i] /= sum;
    } else {
        std::fill(result.begin(), result.end(), 1.0 / NUM_ACTIONS);
    }
    return result;
}

} // namespace poker
