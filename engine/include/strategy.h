#pragma once

#include "card.h"
#include "cfr.h"
#include "subgame.h"
#include <string>

namespace poker {

// ─── Strategy Manager ────────────────────────────────────────
// High-level interface used by the decision server.
// Wraps blueprint + difficulty-based noise + optional subgame solving.

enum class Difficulty {
    NORMAL,    // blueprint + heavy noise + simulated leaks
    MEDIUM,    // blueprint + light exploration
    ADVANCED,  // blueprint + real-time subgame solving
};

struct Decision {
    ActionType action;
    double amount;  // for raise actions, the raise-to amount
    double confidence;
    std::string reasoning;  // brief explanation for debugging
};

class StrategyManager {
public:
    struct Config {
        std::string blueprint_path = "data/blueprint.bin";
        SubgameSolver::Config subgame_config;
    };

    explicit StrategyManager(Config config);

    // Load pre-trained blueprint
    bool load_blueprint(const std::string& path);

    // Get a decision for the current game state
    Decision decide(
        Difficulty difficulty,
        const Card hole[2],
        const Card board[],
        int board_size,
        int pot,
        const int stacks[],
        int current_bet,
        int player_bet,
        int player,
        int num_players,
        const std::vector<ActionType>& valid_actions
    );

private:
    Config config_;
    MCCFRTrainer blueprint_;
    SubgameSolver subgame_solver_;
    HandAbstraction abstraction_;

    // Apply difficulty-specific noise to strategy
    void apply_difficulty(
        Difficulty diff,
        double strategy[NUM_ACTIONS],
        const std::vector<ActionType>& valid_actions
    );
};

} // namespace poker
