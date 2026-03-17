#include "strategy.h"

#include <algorithm>
#include <random>
#include <iostream>
#include <cmath>

namespace poker {

StrategyManager::StrategyManager(Config config)
    : config_(config)
    , blueprint_(MCCFRTrainer::Config{})
    , subgame_solver_(config.subgame_config)
    , abstraction_(HandAbstraction::Config{})
{}

bool StrategyManager::load_blueprint(const std::string& path) {
    bool ok = blueprint_.load(path);
    if (ok) {
        // Build the abstract game tree for node_id resolution
        GameTreeBuilder builder(config_.tree_config);
        game_tree_ = builder.build();
        std::cerr << "Game tree built: " << builder.node_count() << " nodes" << std::endl;
    }
    return ok;
}

uint32_t StrategyManager::resolve_node_id(const std::vector<ActionType>& action_history) const {
    if (!game_tree_ || action_history.empty()) return 0;

    const GameNode* node = game_tree_.get();
    for (ActionType action : action_history) {
        if (node->type == NodeType::TERMINAL) return 0;

        // Find child matching this action
        bool found = false;
        int action_idx = static_cast<int>(action);
        for (size_t i = 0; i < node->valid_actions.size(); ++i) {
            if (node->valid_actions[i] == action && i < node->children.size() && node->children[i]) {
                node = node->children[i].get();
                found = true;
                break;
            }
        }
        if (!found) {
            // Action not in tree — could be a sizing mismatch.
            // Fall back to first valid child as approximation.
            if (!node->children.empty() && node->children[0]) {
                node = node->children[0].get();
            } else {
                return 0;
            }
        }
    }
    return node->node_id;
}

void StrategyManager::apply_difficulty(
    Difficulty diff,
    double strategy[NUM_ACTIONS],
    const std::vector<ActionType>& valid_actions
) {
    static std::mt19937 rng(std::random_device{}());

    if (diff == Difficulty::NORMAL) {
        // Heavy noise + simulated leaks
        double epsilon = 0.3;

        double noise[NUM_ACTIONS] = {};
        double noise_sum = 0;
        for (int a = 0; a < NUM_ACTIONS; ++a) {
            bool valid = false;
            for (auto va : valid_actions) {
                if (static_cast<int>(va) == a) { valid = true; break; }
            }
            if (!valid) { noise[a] = 0; continue; }

            noise[a] = std::uniform_real_distribution<>(0, 1)(rng);
            noise_sum += noise[a];
        }
        if (noise_sum > 0) {
            for (int a = 0; a < NUM_ACTIONS; ++a) noise[a] /= noise_sum;
        }

        for (int a = 0; a < NUM_ACTIONS; ++a) {
            strategy[a] = (1.0 - epsilon) * strategy[a] + epsilon * noise[a];
        }

        // Simulated leaks: call too much, fold too little
        strategy[static_cast<int>(ActionType::CALL)] *= 1.3;
        strategy[static_cast<int>(ActionType::FOLD)] *= 0.7;

        double sum = 0;
        for (int a = 0; a < NUM_ACTIONS; ++a) sum += strategy[a];
        if (sum > 0) {
            for (int a = 0; a < NUM_ACTIONS; ++a) strategy[a] /= sum;
        }

    } else if (diff == Difficulty::MEDIUM) {
        double epsilon = 0.05;
        double noise[NUM_ACTIONS] = {};
        double noise_sum = 0;
        for (int a = 0; a < NUM_ACTIONS; ++a) {
            bool valid = false;
            for (auto va : valid_actions) {
                if (static_cast<int>(va) == a) { valid = true; break; }
            }
            if (!valid) { noise[a] = 0; continue; }
            noise[a] = 1.0;
            noise_sum += 1.0;
        }
        if (noise_sum > 0) {
            for (int a = 0; a < NUM_ACTIONS; ++a) noise[a] /= noise_sum;
        }
        for (int a = 0; a < NUM_ACTIONS; ++a) {
            strategy[a] = (1.0 - epsilon) * strategy[a] + epsilon * noise[a];
        }
    }
    // ADVANCED: no noise — pure subgame solution
}

Decision StrategyManager::decide(
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
    const std::vector<ActionType>& action_history
) {
    double strategy[NUM_ACTIONS] = {};

    if (difficulty == Difficulty::ADVANCED) {
        // Real-time subgame solving with full game state
        int player_bets[6] = {};
        bool folded[6] = {};
        bool all_in[6] = {};
        player_bets[player] = player_bet;

        subgame_solver_.solve(
            blueprint_, hole, board, board_size,
            pot, stacks, current_bet,
            player_bets, folded, all_in,
            player, num_players,
            strategy
        );
    } else {
        // Use blueprint strategy with game tree node mapping
        Street street;
        if (board_size == 0) street = Street::PREFLOP;
        else if (board_size == 3) street = Street::FLOP;
        else if (board_size == 4) street = Street::TURN;
        else street = Street::RIVER;

        InfoSetKey key;
        key.bucket = abstraction_.get_bucket(street, hole, board, board_size);
        key.node_id = resolve_node_id(action_history);

        blueprint_.get_strategy(key, strategy);
    }

    // Apply difficulty noise
    apply_difficulty(difficulty, strategy, valid_actions);

    // Zero out invalid actions and renormalize
    for (int a = 0; a < NUM_ACTIONS; ++a) {
        bool valid = false;
        for (auto va : valid_actions) {
            if (static_cast<int>(va) == a) { valid = true; break; }
        }
        if (!valid) strategy[a] = 0;
    }

    double sum = 0;
    for (int a = 0; a < NUM_ACTIONS; ++a) sum += strategy[a];
    if (sum > 0) {
        for (int a = 0; a < NUM_ACTIONS; ++a) strategy[a] /= sum;
    } else {
        // Fallback: call > check > fold
        for (auto va : valid_actions) {
            if (va == ActionType::CALL) { strategy[static_cast<int>(ActionType::CALL)] = 1; sum = 1; break; }
            if (va == ActionType::CHECK) { strategy[static_cast<int>(ActionType::CHECK)] = 1; sum = 1; break; }
        }
        if (sum == 0) strategy[static_cast<int>(ActionType::FOLD)] = 1;
    }

    // Sample action from strategy
    static std::mt19937 action_rng(std::random_device{}());
    double r = std::uniform_real_distribution<>(0, 1)(action_rng);
    double cumulative = 0;
    ActionType chosen = ActionType::FOLD;
    for (int a = 0; a < NUM_ACTIONS; ++a) {
        cumulative += strategy[a];
        if (r <= cumulative) {
            chosen = static_cast<ActionType>(a);
            break;
        }
    }

    // Calculate raise amount
    double amount = 0;
    if (chosen == ActionType::RAISE_HALF) {
        amount = current_bet + pot * 0.33;
    } else if (chosen == ActionType::RAISE_POT) {
        amount = current_bet + pot * 0.75;
    } else if (chosen == ActionType::RAISE_2X) {
        amount = current_bet + pot * 1.5;
    } else if (chosen == ActionType::CALL) {
        amount = current_bet - player_bet;
    } else if (chosen == ActionType::ALL_IN) {
        amount = stacks[player] + player_bet;
    }

    Decision dec;
    dec.action = chosen;
    dec.amount = amount;
    dec.confidence = strategy[static_cast<int>(chosen)];
    dec.reasoning = std::string(action_name(chosen)) + " with confidence " +
                   std::to_string(dec.confidence);

    return dec;
}

} // namespace poker
