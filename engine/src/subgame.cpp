#include "subgame.h"
#include "hand_eval.h"

#include <chrono>
#include <iostream>
#include <algorithm>
#include <numeric>
#include <cmath>

namespace poker {

SubgameSolver::SubgameSolver(Config config)
    : config_(config)
    , abstraction_(config.abstraction_config)
    , rng_(std::random_device{}())
{}

InfoSetKey SubgameSolver::make_subgame_key(
    int player,
    const Card hole[2],
    const Card board[],
    int board_size,
    const GameNode* node
) const {
    InfoSetKey key;
    key.bucket = abstraction_.get_bucket(node->street, hole, board, board_size);
    key.node_id = node->node_id;
    return key;
}

void SubgameSolver::sample_opponent_hands(
    const Card hero_hole[2],
    const Card board[],
    int board_size,
    Card hands[][2],
    int hero_idx,
    int num_players
) {
    // Build set of used cards (hero + board)
    CardSet used = card_bit(hero_hole[0]) | card_bit(hero_hole[1]);
    for (int i = 0; i < board_size; ++i) used |= card_bit(board[i]);

    // Collect remaining cards
    std::vector<Card> remaining;
    remaining.reserve(52);
    for (int c = 0; c < 52; ++c) {
        if (!(used & card_bit(c))) remaining.push_back(c);
    }
    std::shuffle(remaining.begin(), remaining.end(), rng_);

    // Deal to hero
    hands[hero_idx][0] = hero_hole[0];
    hands[hero_idx][1] = hero_hole[1];

    // Deal to opponents
    int idx = 0;
    for (int p = 0; p < num_players; ++p) {
        if (p == hero_idx) continue;
        hands[p][0] = remaining[idx++];
        hands[p][1] = remaining[idx++];
    }
}

double SubgameSolver::cfr_traverse(
    GameNode* node,
    int traverser,
    const MCCFRTrainer& blueprint,
    const Card hands[][2],
    const Card board[],
    int board_size,
    Street start_street,
    int depth
) {
    // ── Terminal node ──
    if (node->type == NodeType::TERMINAL) {
        int in_hand = 0;
        int last_player = -1;
        for (int i = 0; i < node->num_players; ++i) {
            if (!node->folded[i]) { in_hand++; last_player = i; }
        }

        if (in_hand == 1) {
            return (last_player == traverser)
                ? static_cast<double>(node->pot)
                : static_cast<double>(-node->bets[traverser]);
        }

        // Showdown
        const auto& eval = get_evaluator();
        uint16_t my_rank = eval.evaluate(hands[traverser], board, 5);
        bool i_win = true;
        bool tied = false;

        for (int i = 0; i < node->num_players; ++i) {
            if (i == traverser || node->folded[i]) continue;
            uint16_t opp_rank = eval.evaluate(hands[i], board, 5);
            if (opp_rank < my_rank) { i_win = false; break; }
            if (opp_rank == my_rank) tied = true;
        }

        if (i_win && !tied) {
            return static_cast<double>(node->pot) - node->bets[traverser];
        } else if (tied) {
            int num_tied = 1;
            for (int i = 0; i < node->num_players; ++i) {
                if (i == traverser || node->folded[i]) continue;
                uint16_t r = eval.evaluate(hands[i], board, 5);
                if (r == my_rank) num_tied++;
            }
            return static_cast<double>(node->pot) / num_tied - node->bets[traverser];
        } else {
            return static_cast<double>(-node->bets[traverser]);
        }
    }

    // ── Depth limit: use EHS-based leaf evaluation ──
    int streets_ahead = static_cast<int>(node->street) - static_cast<int>(start_street);
    if (streets_ahead >= config_.depth_limit) {
        // Approximate expected value using EHS
        int bs = 0;
        switch (node->street) {
            case Street::PREFLOP: bs = 0; break;
            case Street::FLOP: bs = 3; break;
            case Street::TURN: bs = 4; break;
            case Street::RIVER: bs = 5; break;
        }
        double ehs = effective_hand_strength(hands[traverser], board, bs, 200);
        // EV ≈ pot × P(win) - invested × P(lose)
        return ehs * node->pot - (1.0 - ehs) * node->bets[traverser];
    }

    int acting = node->player;
    int num_actions = static_cast<int>(node->valid_actions.size());
    if (num_actions == 0) return 0;

    // Board size for current street
    int bs = 0;
    switch (node->street) {
        case Street::PREFLOP: bs = 0; break;
        case Street::FLOP: bs = 3; break;
        case Street::TURN: bs = 4; break;
        case Street::RIVER: bs = 5; break;
    }

    if (acting == traverser) {
        // ── Traverser node: explore all actions, update regrets ──
        InfoSetKey key = make_subgame_key(acting, hands[acting], board, bs, node);
        auto& info = local_info_sets_[key];
        info.num_actions = num_actions;

        double strategy[NUM_ACTIONS];
        info.get_strategy(strategy);

        double action_utils[NUM_ACTIONS] = {};
        double node_util = 0;

        for (int a = 0; a < num_actions; ++a) {
            action_utils[a] = cfr_traverse(
                node->children[a].get(), traverser, blueprint,
                hands, board, board_size, start_street, depth + 1
            );
            node_util += strategy[a] * action_utils[a];
        }

        // Update regrets (CFR+: floor negative to zero)
        for (int a = 0; a < num_actions; ++a) {
            double regret = action_utils[a] - node_util;
            info.regret_sum[a] += regret;
            info.regret_sum[a] = std::max(info.regret_sum[a], 0.0);
        }

        // Update strategy sum
        for (int a = 0; a < num_actions; ++a) {
            info.strategy_sum[a] += strategy[a];
        }

        return node_util;
    } else {
        // ── Opponent node: sample one action from blueprint strategy ──
        InfoSetKey bp_key;
        bp_key.bucket = abstraction_.get_bucket(node->street, hands[acting], board, bs);
        bp_key.node_id = node->node_id;

        double opp_strategy[NUM_ACTIONS];
        blueprint.get_strategy(bp_key, opp_strategy);

        // Mask invalid actions and renormalize
        double valid_sum = 0;
        for (int a = 0; a < num_actions; ++a) {
            valid_sum += opp_strategy[static_cast<int>(node->valid_actions[a])];
        }

        // Sample action
        double r = std::uniform_real_distribution<>(0, 1)(rng_);
        double cumulative = 0;
        int sampled = 0;

        for (int a = 0; a < num_actions; ++a) {
            int act_idx = static_cast<int>(node->valid_actions[a]);
            double prob = (valid_sum > 0)
                ? opp_strategy[act_idx] / valid_sum
                : 1.0 / num_actions;
            cumulative += prob;
            if (r <= cumulative) { sampled = a; break; }
        }

        return cfr_traverse(
            node->children[sampled].get(), traverser, blueprint,
            hands, board, board_size, start_street, depth + 1
        );
    }
}

void SubgameSolver::solve(
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
) {
    auto start = std::chrono::steady_clock::now();

    // Clear local info sets from any previous solve
    local_info_sets_.clear();

    // Determine current street
    Street current_street;
    if (board_size == 0) current_street = Street::PREFLOP;
    else if (board_size == 3) current_street = Street::FLOP;
    else if (board_size == 4) current_street = Street::TURN;
    else current_street = Street::RIVER;

    // Build subgame tree from current state
    GameTreeBuilder::Config tree_cfg;
    tree_cfg.num_players = num_players;
    tree_cfg.starting_stack = stacks[player];  // approximate: use hero stack
    tree_cfg.max_raises_per_street = config_.max_raises_per_street;
    tree_cfg.bet_sizes = config_.bet_sizes;

    GameTreeBuilder builder(tree_cfg);
    auto subtree = builder.build();

    // Inject the actual game state into the root node
    subtree->pot = pot;
    subtree->current_bet = current_bet;
    subtree->num_players = num_players;
    subtree->player = player;
    subtree->street = current_street;
    for (int i = 0; i < num_players; ++i) {
        subtree->stacks[i] = stacks[i];
        subtree->bets[i] = player_bets[i];
        subtree->folded[i] = folded[i];
        subtree->all_in[i] = all_in[i];
    }

    // Complete the board to 5 cards for rollouts
    // (we'll sample the remaining cards each iteration)
    Card full_board[5];
    for (int i = 0; i < board_size; ++i) full_board[i] = board[i];

    // ── Run MCCFR iterations ──
    int iterations = 0;
    while (iterations < config_.max_iterations) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
        if (elapsed_ms >= config_.max_time_ms) break;

        iterations++;

        // Sample opponent hands + remaining board cards
        CardSet used = card_bit(hole[0]) | card_bit(hole[1]);
        for (int i = 0; i < board_size; ++i) used |= card_bit(board[i]);

        std::vector<Card> remaining;
        remaining.reserve(52);
        for (int c = 0; c < 52; ++c) {
            if (!(used & card_bit(c))) remaining.push_back(c);
        }
        std::shuffle(remaining.begin(), remaining.end(), rng_);

        // Deal opponent hands
        Card hands[6][2];
        hands[player][0] = hole[0];
        hands[player][1] = hole[1];
        int idx = 0;
        for (int p = 0; p < num_players; ++p) {
            if (p == player || folded[p]) continue;
            hands[p][0] = remaining[idx++];
            hands[p][1] = remaining[idx++];
        }

        // Complete board
        for (int i = board_size; i < 5; ++i) {
            full_board[i] = remaining[idx++];
        }

        // Traverse for the acting player only
        cfr_traverse(
            subtree.get(), player, blueprint,
            hands, full_board, 5, current_street, 0
        );
    }

    // ── Extract converged strategy from root info set ──
    int bs = 0;
    switch (current_street) {
        case Street::PREFLOP: bs = 0; break;
        case Street::FLOP: bs = 3; break;
        case Street::TURN: bs = 4; break;
        case Street::RIVER: bs = 5; break;
    }

    InfoSetKey root_key = make_subgame_key(player, hole, board, bs, subtree.get());
    auto it = local_info_sets_.find(root_key);
    if (it != local_info_sets_.end()) {
        it->second.get_average_strategy(result_strategy);
    } else {
        // Fallback to blueprint
        InfoSetKey bp_key;
        bp_key.bucket = abstraction_.get_bucket(current_street, hole, board, bs);
        bp_key.node_id = 0;
        blueprint.get_strategy(bp_key, result_strategy);
    }

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();
    std::cout << "Subgame solved: " << iterations << " iterations in "
              << elapsed << "ms | local info sets: " << local_info_sets_.size()
              << std::endl;
}

// ── Simplified interface (backward compat) ──
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
    int player_bets[6] = {};
    bool folded[6] = {};
    bool all_in[6] = {};

    solve(blueprint, hole, board, board_size, pot, stacks, current_bet,
          player_bets, folded, all_in, player, num_players, result_strategy);
}

} // namespace poker
