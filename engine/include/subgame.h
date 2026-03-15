#pragma once

#include "card.h"
#include "cfr.h"
#include "abstraction.h"
#include <string>

namespace poker {

// ─── Subgame Solver ──────────────────────────────────────────
// Real-time depth-limited subgame solving for "Advanced" difficulty.
//
// Given a concrete game state (actual cards, pot, stacks), builds
// a smaller game tree from that point and runs CFR to refine the
// blueprint strategy for this specific situation.

class SubgameSolver {
public:
    struct Config {
        int max_iterations = 500000;
        int max_time_ms = 25000;  // 25s budget (within 30s limit)
        int depth_limit = 2;      // streets to look ahead
        HandAbstraction::Config abstraction_config;
    };

    explicit SubgameSolver(Config config);

    // Solve a subgame starting from the current state.
    // Returns the recommended action distribution.
    //
    // blueprint: the pre-trained blueprint strategy
    // hole: this player's hole cards
    // board: current community cards
    // board_size: number of community cards
    // pot: current pot size
    // stacks: each player's remaining stack
    // current_bet: highest bet this street
    // player: acting player index
    // num_players: total players
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
};

} // namespace poker
