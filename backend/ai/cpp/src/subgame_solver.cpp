/**
 * Subgame solver — depth-limited re-solving for Hard difficulty.
 */

#include "subgame_solver.h"
#include "hand_abstraction.h"
#include <algorithm>
#include <random>
#include <sstream>
#include <unordered_map>
#include <numeric>

namespace poker {

namespace {

thread_local std::mt19937 rng{std::random_device{}()};

// Simplified subgame node
struct SubNode {
    std::array<double, NUM_ACTIONS> regret_sum{};
    std::array<double, NUM_ACTIONS> strategy_sum{};

    std::array<double, NUM_ACTIONS> get_strategy() const {
        std::array<double, NUM_ACTIONS> s{};
        double ps = 0;
        for (int a = 0; a < NUM_ACTIONS; ++a) { s[a] = std::max(regret_sum[a], 0.0); ps += s[a]; }
        if (ps > 0) for (auto& v : s) v /= ps;
        else std::fill(s.begin(), s.end(), 1.0 / NUM_ACTIONS);
        return s;
    }

    std::array<double, NUM_ACTIONS> get_average() const {
        std::array<double, NUM_ACTIONS> a{};
        double t = std::accumulate(strategy_sum.begin(), strategy_sum.end(), 0.0);
        if (t > 0) for (int i = 0; i < NUM_ACTIONS; ++i) a[i] = strategy_sum[i] / t;
        else std::fill(a.begin(), a.end(), 1.0 / NUM_ACTIONS);
        return a;
    }
};

struct SubState {
    double ehs_hero;
    int pot;
    int to_act;     // 0=hero, 1=villain
    int bets;
    int depth;
    bool terminal;
    int winner;     // -1=ongoing, 0=hero, 1=villain, 2=showdown
    std::string history;
    std::array<int, 2> invested;
};

std::vector<int> sub_valid(const SubState& s) {
    std::vector<int> a;
    if (s.bets > 0) a.push_back(0); // fold
    a.push_back(1); // check/call
    if (s.bets < 4) {
        a.push_back(2); // half pot
        a.push_back(3); // pot
        if (s.bets < 3) a.push_back(4); // 2x pot
    }
    a.push_back(5); // all-in, always available
    return a;
}

SubState sub_apply(const SubState& s, int action) {
    SubState n = s;
    n.depth++;
    n.history += std::to_string(action);

    if (action == 0) { n.terminal = true; n.winner = 1 - s.to_act; return n; }
    if (action == 1) {
        int call = s.invested[1 - s.to_act] - s.invested[s.to_act];
        n.invested[s.to_act] += call;
        n.pot += call;
        if (s.bets > 0) { n.terminal = true; n.winner = 2; }
        else n.to_act = 1 - s.to_act;
        return n;
    }
    int bet = (action == 2) ? n.pot / 2 : n.pot;
    bet = std::max(bet, 2);
    n.invested[s.to_act] += bet;
    n.pot += bet;
    n.bets++;
    n.to_act = 1 - s.to_act;
    return n;
}

double sub_cfr(
    const SubState& state, int traverser, double ehs_opp,
    double pi_t, double pi_o, int depth_limit,
    std::unordered_map<std::string, SubNode>& nodes
) {
    if (state.terminal || state.depth >= depth_limit) {
        if (state.winner == 2 || state.depth >= depth_limit) {
            double hero_ehs = (traverser == 0) ? state.ehs_hero : ehs_opp;
            double vill_ehs = (traverser == 0) ? ehs_opp : state.ehs_hero;
            if (hero_ehs > vill_ehs) return static_cast<double>(state.invested[1 - traverser]);
            if (vill_ehs > hero_ehs) return -static_cast<double>(state.invested[traverser]);
            return 0;
        }
        double u = (state.winner == traverser)
            ? static_cast<double>(state.invested[1 - traverser])
            : -static_cast<double>(state.invested[traverser]);
        return u;
    }

    auto valid = sub_valid(state);
    std::string key = std::to_string(state.to_act) + ":" + state.history;
    SubNode& node = nodes[key];
    auto strat = node.get_strategy();

    if (state.to_act == traverser) {
        std::array<double, NUM_ACTIONS> au{};
        double nu = 0;
        for (int a : valid) {
            SubState next = sub_apply(state, a);
            au[a] = sub_cfr(next, traverser, ehs_opp, pi_t * strat[a], pi_o, depth_limit, nodes);
            nu += strat[a] * au[a];
        }
        for (int a : valid) node.regret_sum[a] += pi_o * (au[a] - nu);
        return nu;
    } else {
        std::uniform_real_distribution<double> d(0, 1);
        double r = d(rng), c = 0;
        int sa = valid.back();
        for (int a : valid) { c += strat[a]; if (r < c) { sa = a; break; } }
        for (int a : valid) node.strategy_sum[a] += pi_o * strat[a];
        SubState next = sub_apply(state, sa);
        return sub_cfr(next, traverser, ehs_opp, pi_t, pi_o * strat[sa], depth_limit, nodes);
    }
}

} // anonymous namespace

SubgameSolver::SubgameSolver(const MCCFRTrainer& bp, int dl)
    : blueprint_(bp), depth_limit_(dl) {}

std::array<double, NUM_ACTIONS> SubgameSolver::solve(
    const SubgameState& gs, int iterations
) {
    // Compute hero EHS
    double hero_ehs = compute_ehs(gs.hole_cards, gs.board, 500);
    // Estimate opponent EHS as complement (simplified)
    double opp_ehs = 1.0 - hero_ehs;

    SubState root{};
    root.ehs_hero = hero_ehs;
    root.pot = gs.pot;
    root.to_act = 0;
    root.bets = (gs.current_bet > 0) ? 1 : 0;
    root.depth = 0;
    root.terminal = false;
    root.winner = -1;
    root.invested = {0, gs.current_bet};

    std::unordered_map<std::string, SubNode> nodes;

    for (int i = 0; i < iterations; ++i) {
        for (int p = 0; p < 2; ++p) {
            sub_cfr(root, p, opp_ehs, 1.0, 1.0, depth_limit_, nodes);
        }
    }

    // Return hero's root strategy
    std::string root_key = "0:";
    auto it = nodes.find(root_key);
    if (it != nodes.end()) return it->second.get_average();

    std::array<double, NUM_ACTIONS> uniform{};
    std::fill(uniform.begin(), uniform.end(), 1.0 / NUM_ACTIONS);
    return uniform;
}

} // namespace poker
