#pragma once

#include "card.h"
#include "cfr.h"
#include "abstraction.h"
#include "game_tree.h"
#include <string>
#include <unordered_map>
#include <random>
#include <chrono>

namespace poker {

// ─── Subgame Solver ──────────────────────────────────────────
// Real-time depth-limited subgame solving.
//
// Given a concrete game state (actual cards, pot, stacks), builds
// a smaller game tree from that point and runs MCCFR (CFR+) to
// compute a refined strategy for the acting player.
//
// Key design (Pluribus-inspired):
// - Opponents are assumed to follow the blueprint strategy
// - Only the acting player's strategy is refined
// - Depth-limited: look ahead at most `depth_limit` streets
// - Beyond the depth limit, use blueprint expected values

class SubgameSolver {
public:
    struct Config {
        int max_iterations = 500000;
        int max_time_ms = 5000;       // 5s budget for real-time use
        int depth_limit = 2;          // streets to look ahead
        int max_raises_per_street = 3;
        std::vector<double> bet_sizes = {0.33, 0.75, 1.5};
        HandAbstraction::Config abstraction_config;
    };

    explicit SubgameSolver(Config config);

    // Solve a subgame starting from the current state.
    // Returns the recommended action distribution for the acting player.
    //
    // blueprint: the pre-trained blueprint strategy (used for opponent play)
    // hole: this player's hole cards
    // board: current community cards
    // board_size: number of community cards (0/3/4/5)
    // pot: current pot size
    // stacks: each player's remaining stack
    // current_bet: highest bet this street
    // player_bets: each player's current bet this street
    // folded: which players have folded
    // all_in: which players are all-in
    // player: acting player index
    // num_players: total players
    // result_strategy: output array of action probabilities
    void solve(
        const MCCFRTrainer& blueprint,
        const Card hole[2],
        const Card board[],
        int board_size,
        int pot,
        const int stacks[],
        int current_bet,
        const int player_bets[],
        const bool folded[],
        const bool all_in[],
        int player,
        int num_players,
        double result_strategy[NUM_ACTIONS]
    );

    // Simplified interface (backward compat) — creates default bets/folded/all_in
    void solve(
        const MCCFRTrainer& blueprint,
        const Card hole[2],
        const Card board[],
        int board_size,
        int pot,
        const int stacks[],
        int current_bet,
        int player,
        int num_players,
        double result_strategy[NUM_ACTIONS]
    );

private:
    Config config_;
    HandAbstraction abstraction_;

    // Local info sets for the subgame CFR
    std::unordered_map<InfoSetKey, InfoSetData, InfoSetKeyHash> local_info_sets_;

    std::mt19937 rng_;

    // MCCFR traverse on the subgame tree
    // Only updates regrets for `traverser`; opponents sample from blueprint
    double cfr_traverse(
        GameNode* node,
        int traverser,
        const MCCFRTrainer& blueprint,
        const Card hands[][2],
        const Card board[],
        int board_size,
        Street start_street,
        int depth
    );

    // Sample opponent hands that are consistent with remaining deck
    void sample_opponent_hands(
        const Card hero_hole[2],
        const Card board[],
        int board_size,
        Card hands[][2],
        int hero_idx,
        int num_players
    );

    // Build info set key for subgame (uses concrete board, not just node_id)
    InfoSetKey make_subgame_key(
        int player,
        const Card hole[2],
        const Card board[],
        int board_size,
        const GameNode* node
    ) const;
};

} // namespace poker
