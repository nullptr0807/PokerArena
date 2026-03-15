/**
 * MCCFR implementation — External Sampling CFR for 2-player NLHE.
 */

#include "mccfr.h"
#include "hand_abstraction.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <bitset>
#include <numeric>
#include <random>
#include <sstream>
#include <fstream>
#include <thread>
#include <unordered_map>
#include <vector>

namespace poker {

namespace {

thread_local std::mt19937 rng{std::random_device{}()};

enum AbsAction : int {
    FOLD = 0, CHECK_CALL = 1, BET_30 = 2, BET_50 = 3,
    BET_70 = 4, BET_POT = 5, BET_120 = 6, ALL_IN = 7,
};
const char* ACTION_NAMES[] = {"f", "k", "3", "5", "7", "p", "x", "a"};

struct CFRState {
    std::array<std::array<int, 2>, 2> holes;
    std::array<int, 5> board;
    int board_count;
    int pot;
    int street;
    int acting_player;
    int bets_this_street;
    bool terminal;
    int winner;
    std::string history;
    std::array<int, 2> invested;
    // Cached buckets per player per street (computed once per street transition)
    std::array<std::array<int, 4>, 2> buckets; // [player][street]
    int buckets_computed_street;  // highest street for which buckets are valid
};

CFRState deal_hand() {
    CFRState s{};
    std::array<int, 52> deck;
    std::iota(deck.begin(), deck.end(), 0);
    for (int i = 51; i > 0; --i) {
        std::uniform_int_distribution<int> d(0, i);
        std::swap(deck[i], deck[d(rng)]);
    }
    s.holes[0] = {deck[0], deck[1]};
    s.holes[1] = {deck[2], deck[3]};
    s.board = {deck[4], deck[5], deck[6], deck[7], deck[8]};
    s.board_count = 0;
    s.pot = 3;
    s.invested = {1, 2};
    s.street = 0;
    s.acting_player = 0;
    s.bets_this_street = 1;
    s.terminal = false;
    s.winner = -1;
    // Precompute preflop buckets (no board needed)
    std::vector<int> empty_board;
    for (int p = 0; p < 2; ++p) {
        s.buckets[p][0] = hand_to_bucket(s.holes[p], empty_board);
    }
    s.buckets_computed_street = 0;
    return s;
}

std::vector<int> get_valid_actions(const CFRState& s) {
    std::vector<int> actions;
    if (s.bets_this_street > 0) actions.push_back(FOLD);
    actions.push_back(CHECK_CALL);
    if (s.bets_this_street < 3) {
        actions.push_back(BET_30);
        actions.push_back(BET_50);
        actions.push_back(BET_70);
        actions.push_back(BET_POT);
        actions.push_back(BET_120);
    }
    if (s.bets_this_street >= 2) actions.push_back(ALL_IN);
    return actions;
}

void ensure_buckets(CFRState& s) {
    if (s.street > s.buckets_computed_street) {
        std::vector<int> bc(s.board.begin(), s.board.begin() + s.board_count);
        for (int p = 0; p < 2; ++p) {
            s.buckets[p][s.street] = hand_to_bucket(s.holes[p], bc);
        }
        s.buckets_computed_street = s.street;
    }
}

// Encode info key as uint64: street(2bit) | bucket(8bit) | history_hash(54bit)
uint64_t make_info_key(CFRState& s, int player) {
    ensure_buckets(s);
    int bucket = s.buckets[player][s.street];
    // FNV-1a hash of history string
    uint64_t h = 14695981039346656037ULL;
    for (char c : s.history) {
        h ^= static_cast<uint64_t>(c);
        h *= 1099511628211ULL;
    }
    return (static_cast<uint64_t>(s.street) << 62)
         | (static_cast<uint64_t>(bucket & 0xFF) << 54)
         | (h & 0x003FFFFFFFFFFFFFULL);
}

CFRState apply_action(const CFRState& s, int action) {
    CFRState next = s;
    next.history += ACTION_NAMES[action];

    if (action == FOLD) {
        next.terminal = true;
        next.winner = 1 - s.acting_player;
        return next;
    }
    if (action == CHECK_CALL) {
        int to_call = s.invested[1 - s.acting_player] - s.invested[s.acting_player];
        next.invested[s.acting_player] += to_call;
        next.pot += to_call;
        bool street_ends = (s.bets_this_street == 0 && next.history.length() >= 2 &&
                            next.history.back() == 'k' && next.history[next.history.size()-2] == 'k') ||
                           (s.bets_this_street > 0);
        // Simpler: if both players have acted and bets are matched
        if (s.bets_this_street > 0 || (s.bets_this_street == 0 && s.acting_player == 1)) {
            if (next.street == 3) {
                next.terminal = true;
                next.winner = 2;
            } else {
                next.street++;
                if (next.street == 1) next.board_count = 3;
                else if (next.street == 2) next.board_count = 4;
                else next.board_count = 5;
                next.bets_this_street = 0;
                next.acting_player = 0;
                next.history += "/";
            }
        } else {
            next.acting_player = 1 - s.acting_player;
        }
        return next;
    }
    // Bet/raise
    int bet = 0;
    if (action == BET_30) bet = next.pot * 30 / 100;
    else if (action == BET_50) bet = next.pot * 50 / 100;
    else if (action == BET_70) bet = next.pot * 70 / 100;
    else if (action == BET_POT) bet = next.pot;
    else if (action == BET_120) bet = next.pot * 120 / 100;
    else if (action == ALL_IN) bet = 200;
    bet = std::max(bet, 2);
    next.invested[s.acting_player] += bet;
    next.pot += bet;
    next.bets_this_street++;
    next.acting_player = 1 - s.acting_player;
    return next;
}

// ─── Lightweight exact 7-card evaluator for showdown ───────────
// Returns a rank where LOWER = BETTER. Enumerates all C(7,5)=21
// five-card combos and picks the best.
namespace {
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
uint16_t evaluate_7(int cards[7]) {
    uint16_t best=0xFFFF;
    int idx[7]={0,1,2,3,4,5,6};
    for(int i=0;i<7-4;i++)for(int j=i+1;j<7-3;j++)for(int k=j+1;k<7-2;k++)
        for(int l=k+1;l<7-1;l++)for(int m=l+1;m<7;m++){
            int r[5]={cards[i]%13,cards[j]%13,cards[k]%13,cards[l]%13,cards[m]%13};
            int su[5]={cards[i]/13,cards[j]/13,cards[k]/13,cards[l]/13,cards[m]/13};
            uint16_t v=eval_5_exact(r,su); if(v<best)best=v;
        }
    return best;
}
} // anon namespace

double showdown_utility(const CFRState& s) {
    int cards0[7] = {s.holes[0][0], s.holes[0][1], s.board[0], s.board[1], s.board[2], s.board[3], s.board[4]};
    int cards1[7] = {s.holes[1][0], s.holes[1][1], s.board[0], s.board[1], s.board[2], s.board[3], s.board[4]};
    uint16_t rank0 = evaluate_7(cards0);  // lower = better
    uint16_t rank1 = evaluate_7(cards1);
    if (rank0 < rank1) return static_cast<double>(s.invested[1]);
    if (rank1 < rank0) return -static_cast<double>(s.invested[0]);
    return 0.0;
}

// Recursive CFR traversal (thread-safe via trainer's mutex)
double cfr_traverse(
    CFRState& state, int traverser,
    double pi_t, double pi_o,
    std::unordered_map<uint64_t, InfoNode>& nodes,
    std::shared_mutex* mtx = nullptr,
    int64_t iteration = 0
) {
    if (state.terminal) {
        if (state.winner == 2) {
            double u = showdown_utility(state);
            return traverser == 0 ? u : -u;
        }
        double u = (state.winner == 0)
            ? static_cast<double>(state.invested[1])
            : -static_cast<double>(state.invested[0]);
        return traverser == 0 ? u : -u;
    }
    if (state.history.size() > 15) return 0.0;

    auto valid = get_valid_actions(state);
    uint64_t key = make_info_key(state, state.acting_player);

    // Get or create node (thread-safe if mtx provided)
    InfoNode* node_ptr;
    {
        if (mtx) {
            // Try read first
            std::shared_lock<std::shared_mutex> rlock(*mtx);
            auto it = nodes.find(key);
            if (it != nodes.end()) {
                node_ptr = &it->second;
                rlock.unlock();
            } else {
                rlock.unlock();
                std::unique_lock<std::shared_mutex> wlock(*mtx);
                node_ptr = &nodes[key];  // insert
            }
        } else {
            node_ptr = &nodes[key];
        }
    }
    InfoNode& node = *node_ptr;
    auto strategy = node.get_strategy();

    if (state.acting_player == traverser) {
        std::array<double, NUM_ACTIONS> au{};
        double nu = 0;
        for (int a : valid) {
            CFRState next = apply_action(state, a);
            au[a] = cfr_traverse(next, traverser, pi_t * strategy[a], pi_o, nodes, mtx, iteration);
            nu += strategy[a] * au[a];
        }
        // MCCFR+ regret update: clamp to zero
        for (int a : valid) {
            node.regret_sum[a] = std::max(node.regret_sum[a] + pi_o * (au[a] - nu), 0.0);
        }
        return nu;
    } else {
        std::uniform_real_distribution<double> dist(0.0, 1.0);
        double r = dist(rng), cumul = 0;
        int sampled = valid.back();
        for (int a : valid) {
            cumul += strategy[a];
            if (r < cumul) { sampled = a; break; }
        }
        // MCCFR+ strategy sum: weight by iteration (linear CFR)
        double weight = std::max(iteration, (int64_t)1);
        for (int a : valid) {
            node.strategy_sum[a] += weight * pi_o * strategy[a];
        }
        CFRState next = apply_action(state, sampled);
        return cfr_traverse(next, traverser, pi_t, pi_o * strategy[sampled], nodes, mtx, iteration);
    }
}

} // anonymous namespace

// ─── InfoNode ────────────────────────────────────────────

std::array<double, NUM_ACTIONS> InfoNode::get_strategy() const {
    std::array<double, NUM_ACTIONS> s{};
    double ps = 0;
    for (int a = 0; a < NUM_ACTIONS; ++a) { s[a] = std::max(regret_sum[a], 0.0); ps += s[a]; }
    if (ps > 0) { for (auto& v : s) v /= ps; }
    else std::fill(s.begin(), s.end(), 1.0 / NUM_ACTIONS);
    return s;
}

std::array<double, NUM_ACTIONS> InfoNode::get_average_strategy() const {
    std::array<double, NUM_ACTIONS> a{};
    double t = std::accumulate(strategy_sum.begin(), strategy_sum.end(), 0.0);
    if (t > 0) { for (int i = 0; i < NUM_ACTIONS; ++i) a[i] = strategy_sum[i] / t; }
    else std::fill(a.begin(), a.end(), 1.0 / NUM_ACTIONS);
    return a;
}

// ─── MCCFRTrainer ────────────────────────────────────────

MCCFRTrainer::MCCFRTrainer(int np) : num_players_(np) {}

void MCCFRTrainer::train(int64_t iterations) {
    train_parallel(iterations, 1);
}

void MCCFRTrainer::train_parallel(int64_t iterations, int num_threads) {
    if (num_threads <= 1) {
        for (int64_t i = 0; i < iterations; ++i) {
            for (int p = 0; p < 2; ++p) {
                cfr_impl(p, i);
            }
        }
        return;
    }
    std::atomic<int64_t> counter{0};
    auto worker = [&]() {
        while (true) {
            int64_t idx = counter.fetch_add(1);
            if (idx >= iterations) break;
            for (int p = 0; p < 2; ++p) {
                CFRState state = deal_hand();
                cfr_traverse(state, p, 1.0, 1.0, nodes_, &nodes_mtx_, idx);
            }
        }
    };
    std::vector<std::thread> threads;
    threads.reserve(num_threads);
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back(worker);
    }
    for (auto& th : threads) th.join();
}

double MCCFRTrainer::cfr_impl(int traverser, int64_t iteration) {
    CFRState state = deal_hand();
    return cfr_traverse(state, traverser, 1.0, 1.0, nodes_, nullptr, iteration);
}

std::array<double, NUM_ACTIONS> MCCFRTrainer::query(const std::string& k) const {
    // String-based query: hash the string key the same way engine_bridge uses it
    // For now, uniform fallback — real queries should use query_u64
    std::array<double, NUM_ACTIONS> u{};
    std::fill(u.begin(), u.end(), 1.0 / NUM_ACTIONS);
    return u;
}

std::array<double, NUM_ACTIONS> MCCFRTrainer::query_u64(uint64_t k) const {
    auto it = nodes_.find(k);
    if (it != nodes_.end()) return it->second.get_average_strategy();
    std::array<double, NUM_ACTIONS> u{};
    std::fill(u.begin(), u.end(), 1.0 / NUM_ACTIONS);
    return u;
}

void MCCFRTrainer::save(const std::string& path) const {
    std::ofstream out(path, std::ios::binary);
    uint64_t n = nodes_.size();
    out.write(reinterpret_cast<const char*>(&n), sizeof(n));
    for (const auto& [key, node] : nodes_) {
        out.write(reinterpret_cast<const char*>(&key), sizeof(key));
        out.write(reinterpret_cast<const char*>(node.regret_sum.data()), sizeof(node.regret_sum));
        out.write(reinterpret_cast<const char*>(node.strategy_sum.data()), sizeof(node.strategy_sum));
    }
}

void MCCFRTrainer::load(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return;
    uint64_t n = 0;
    in.read(reinterpret_cast<char*>(&n), sizeof(n));
    nodes_.clear();
    nodes_.reserve(n);
    for (uint64_t i = 0; i < n; ++i) {
        uint64_t key = 0;
        in.read(reinterpret_cast<char*>(&key), sizeof(key));
        InfoNode node{};
        in.read(reinterpret_cast<char*>(node.regret_sum.data()), sizeof(node.regret_sum));
        in.read(reinterpret_cast<char*>(node.strategy_sum.data()), sizeof(node.strategy_sum));
        nodes_[key] = node;
    }
}

} // namespace poker
