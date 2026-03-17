#pragma once

#include "card.h"
#include "cfr.h"
#include "subgame.h"
#include "game_tree.h"
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
        GameTreeBuilder::Config tree_config;  // for building the abstract game tree
    };

    explicit StrategyManager(Config config);

    // Load pre-trained blueprint (also builds game tree for node mapping)
    bool load_blueprint(const std::string& path);

    // Get a decision for the current game state.
    // action_history: sequence of actions taken so far this hand,
    // used to traverse the game tree and find the correct node_id
    // for blueprint strategy lookup.
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
        const std::vector<ActionType>& valid_actions,
        const std::vector<ActionType>& action_history = {}
    );

private:
    Config config_;
    MCCFRTrainer blueprint_;
    SubgameSolver subgame_solver_;
    HandAbstraction abstraction_;

    // Abstract game tree (built once on load_blueprint)
    std::unique_ptr<GameNode> game_tree_;

    // Walk the game tree following action_history, return the node_id.
    // Returns 0 if tree is not built or path is invalid.
    uint32_t resolve_node_id(const std::vector<ActionType>& action_history) const;

    // Apply difficulty-specific noise to strategy
    void apply_difficulty(
        Difficulty diff,
        double strategy[NUM_ACTIONS],
        const std::vector<ActionType>& valid_actions
    );
};

} // namespace poker
