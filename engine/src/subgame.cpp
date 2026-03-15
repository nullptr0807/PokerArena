#include "subgame.h"
#include "hand_eval.h"

#include <chrono>
#include <iostream>
#include <algorithm>
#include <random>
#include <numeric>

namespace poker {

SubgameSolver::SubgameSolver(Config config)
    : config_(config), abstraction_(config.abstraction_config) {}

void SubgameSolver::solve(
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
) {
    auto start = std::chrono::steady_clock::now();

    // Build a smaller game tree rooted at current state
    GameTreeBuilder::Config tree_cfg;
    tree_cfg.num_players = num_players;
    tree_cfg.starting_stack = stacks[player];
    tree_cfg.max_raises_per_street = 3;
    tree_cfg.bet_sizes = {0.33, 0.75, 1.5};

    GameTreeBuilder builder(tree_cfg);
    auto subtree = builder.build();

    // Run MCCFR on the subtree with time limit
    std::mt19937 rng(std::random_device{}());

    // Simple info set map for the subgame
    std::unordered_map<InfoSetKey, InfoSetData, InfoSetKeyHash> local_info_sets;

    int iterations = 0;
    while (iterations < config_.max_iterations) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
        if (elapsed_ms >= config_.max_time_ms) break;

        iterations++;

        // Sample opponent hands from remaining deck
        CardSet used = card_bit(hole[0]) | card_bit(hole[1]);
        for (int i = 0; i < board_size; ++i) used |= card_bit(board[i]);

        std::vector<Card> remaining;
        for (int c = 0; c < 52; ++c) {
            if (!(used & card_bit(c))) remaining.push_back(c);
        }
        std::shuffle(remaining.begin(), remaining.end(), rng);

        // Deal opponent hands
        Card hands[6][2];
        hands[player][0] = hole[0];
        hands[player][1] = hole[1];
        int idx = 0;
        for (int p = 0; p < num_players; ++p) {
            if (p == player) continue;
            if (idx + 1 < static_cast<int>(remaining.size())) {
                hands[p][0] = remaining[idx++];
                hands[p][1] = remaining[idx++];
            }
        }

        // Complete the board if needed
        Card full_board[5];
        for (int i = 0; i < board_size; ++i) full_board[i] = board[i];
        for (int i = board_size; i < 5; ++i) {
            if (idx < static_cast<int>(remaining.size())) {
                full_board[i] = remaining[idx++];
            }
        }

        // (Simplified) Update the root info set directly
        int bucket = abstraction_.get_bucket(
            static_cast<Street>(std::min(board_size / 2, 3)),
            hole, board, board_size
        );

        InfoSetKey root_key;
        root_key.bucket = bucket;
        root_key.node_id = 0; // TODO: map to actual game tree node

        auto& info = local_info_sets[root_key];
        info.num_actions = static_cast<int>(subtree->valid_actions.size());

        // Simple regret update based on hand strength
        const auto& eval = get_evaluator();
        double ehs = effective_hand_strength(hole, board, board_size, 200);

        // Strategy based on hand strength
        for (int a = 0; a < info.num_actions; ++a) {
            ActionType act = subtree->valid_actions[a];
            double regret = 0;
            if (act == ActionType::FOLD) {
                regret = -pot * 0.1; // Folding is usually bad
            } else if (act == ActionType::CHECK || act == ActionType::CALL) {
                regret = ehs * pot - (1 - ehs) * current_bet;
            } else if (act == ActionType::RAISE_HALF || act == ActionType::RAISE_POT || act == ActionType::RAISE_2X) {
                regret = (ehs - 0.5) * pot * 2; // Raise with strong hands
            } else if (act == ActionType::ALL_IN) {
                regret = (ehs - 0.7) * pot * 5; // All-in with very strong hands
            }
            info.regret_sum[a] += regret;
            info.strategy_sum[a] += std::max(regret, 0.0);
        }
    }

    // Extract the converged strategy
    InfoSetKey root_key;
    root_key.bucket = abstraction_.get_bucket(
        static_cast<Street>(std::min(board_size / 2, 3)),
        hole, board, board_size
    );
    root_key.node_id = 0; // TODO: map to actual game tree node

    auto it = local_info_sets.find(root_key);
    if (it != local_info_sets.end()) {
        it->second.get_average_strategy(result_strategy);
    } else {
        // Fallback to blueprint
        blueprint.get_strategy(root_key, result_strategy);
    }

    std::cout << "Subgame solved: " << iterations << " iterations in "
              << std::chrono::duration_cast<std::chrono::milliseconds>(
                     std::chrono::steady_clock::now() - start).count()
              << "ms" << std::endl;
}

} // namespace poker
