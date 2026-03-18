#include "cfr.h"
#include "hand_eval.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <sstream>

namespace poker {

// ─── InfoSetData ─────────────────────────────────────────────

void InfoSetData::get_strategy(double strategy[NUM_ACTIONS]) const {
    // Regret matching: strategy proportional to positive regrets
    double pos_sum = 0;
    for (int a = 0; a < num_actions; ++a) {
        strategy[a] = std::max(regret_sum[a], 0.0);
        pos_sum += strategy[a];
    }
    if (pos_sum > 0) {
        for (int a = 0; a < num_actions; ++a) strategy[a] /= pos_sum;
    } else {
        // Uniform strategy
        for (int a = 0; a < num_actions; ++a) strategy[a] = 1.0 / num_actions;
    }
    // Zero out invalid actions
    for (int a = num_actions; a < NUM_ACTIONS; ++a) strategy[a] = 0;
}

void InfoSetData::get_average_strategy(double strategy[NUM_ACTIONS]) const {
    double total = 0;
    for (int a = 0; a < num_actions; ++a) total += strategy_sum[a];
    if (total > 0) {
        for (int a = 0; a < num_actions; ++a) strategy[a] = strategy_sum[a] / total;
    } else {
        for (int a = 0; a < num_actions; ++a) strategy[a] = 1.0 / num_actions;
    }
    for (int a = num_actions; a < NUM_ACTIONS; ++a) strategy[a] = 0;
}

// ─── MCCFRTrainer ────────────────────────────────────────────

MCCFRTrainer::MCCFRTrainer(Config config)
    : config_(config)
    , abstraction_(config.abstraction_config)
    , rng_(std::random_device{}())
{
    config_.tree_config.num_players = config_.num_players;
    // Don't build tree in constructor — it's only needed for train()
}

bool MCCFRTrainer::load_abstraction(const std::string& path) {
    return abstraction_.load(path);
}

void MCCFRTrainer::sample_deal(Card hands[][2], Card board[5], int num_players) {
    // Shuffle deck
    std::array<Card, 52> deck;
    std::iota(deck.begin(), deck.end(), 0);
    std::shuffle(deck.begin(), deck.end(), rng_);

    int idx = 0;
    for (int p = 0; p < num_players; ++p) {
        hands[p][0] = deck[idx++];
        hands[p][1] = deck[idx++];
    }
    for (int i = 0; i < 5; ++i) {
        board[i] = deck[idx++];
    }
}

InfoSetKey MCCFRTrainer::make_key(
    int player, const Card hole[2],
    const Card board[], int board_size,
    const GameNode* node
) const {
    InfoSetKey key;
    key.bucket = abstraction_.get_bucket(node->street, hole, board, board_size);
    key.node_id = node->node_id;
    return key;
}

double MCCFRTrainer::cfr_traverse(
    GameNode* node,
    int traverser,
    const Card hands[][2],
    const Card board[],
    int board_size,
    double reach_prob[],
    int depth
) {
    if (node->type == NodeType::TERMINAL) {
        // Calculate payoff for traverser
        // Count non-folded players
        int in_hand = 0;
        int last_player = -1;
        for (int i = 0; i < node->num_players; ++i) {
            if (!node->folded[i]) { in_hand++; last_player = i; }
        }

        if (in_hand == 1) {
            // Everyone folded to one player
            if (last_player == traverser) {
                return node->pot;
            } else {
                return -node->bets[traverser];
            }
        }

        // Showdown
        const auto& eval = get_evaluator();
        int actual_board_size = 0;
        switch (node->street) {
            case Street::PREFLOP: actual_board_size = 0; break;
            case Street::FLOP: actual_board_size = 3; break;
            case Street::TURN: actual_board_size = 4; break;
            case Street::RIVER: actual_board_size = 5; break;
        }
        // Use full board for showdown
        actual_board_size = 5;

        uint16_t my_rank = eval.evaluate(hands[traverser], board, actual_board_size);
        bool i_win = true;
        bool tied = false;

        for (int i = 0; i < node->num_players; ++i) {
            if (i == traverser || node->folded[i]) continue;
            uint16_t opp_rank = eval.evaluate(hands[i], board, actual_board_size);
            if (opp_rank < my_rank) { i_win = false; break; }
            if (opp_rank == my_rank) tied = true;
        }

        if (i_win && !tied) {
            return node->pot - node->bets[traverser];
        } else if (tied) {
            // Split pot among tied players
            int num_tied = 1; // traverser
            for (int i = 0; i < node->num_players; ++i) {
                if (i == traverser || node->folded[i]) continue;
                uint16_t r = eval.evaluate(hands[i], board, actual_board_size);
                if (r == my_rank) num_tied++;
            }
            return static_cast<double>(node->pot) / num_tied - node->bets[traverser];
        } else {
            return -node->bets[traverser];
        }
    }

    int acting = node->player;
    int num_actions = static_cast<int>(node->valid_actions.size());
    if (num_actions == 0) return 0;

    // Determine board size for this street
    int bs = 0;
    switch (node->street) {
        case Street::PREFLOP: bs = 0; break;
        case Street::FLOP: bs = 3; break;
        case Street::TURN: bs = 4; break;
        case Street::RIVER: bs = 5; break;
    }

    InfoSetKey key = make_key(acting, hands[acting], board, bs, node);
    auto& info = info_sets_[key];
    info.num_actions = num_actions;

    double strategy[NUM_ACTIONS];
    info.get_strategy(strategy);

    if (acting == traverser) {
        // Traverse all actions, compute counterfactual values
        double action_utils[NUM_ACTIONS] = {};
        double node_util = 0;

        for (int a = 0; a < num_actions; ++a) {
            double new_reach[6];
            std::copy(reach_prob, reach_prob + 6, new_reach);
            new_reach[acting] *= strategy[a];

            action_utils[a] = cfr_traverse(
                node->children[a].get(), traverser,
                hands, board, board_size, new_reach, depth + 1
            );
            node_util += strategy[a] * action_utils[a];
        }

        // Update regrets (External Sampling: no opp_reach weighting)
        for (int a = 0; a < num_actions; ++a) {
            double regret = action_utils[a] - node_util;
            info.regret_sum[a] += regret;

            // CFR+: immediately floor negative regrets to zero
            if (config_.variant == CFRVariant::CFR_PLUS) {
                info.regret_sum[a] = std::max(info.regret_sum[a], 0.0);
            }
        }

        // Update strategy sum at traverser node (correct for External Sampling)
        double my_reach = reach_prob[traverser];
        double weight = my_reach;
        if (config_.variant == CFRVariant::CFR_PLUS ||
            config_.variant == CFRVariant::LINEAR) {
            weight *= static_cast<double>(total_iterations_ + 1);
        }
        for (int a = 0; a < num_actions; ++a) {
            info.strategy_sum[a] += weight * strategy[a];
        }

        return node_util;
    } else {
        // External sampling: sample one action according to strategy
        double r = std::uniform_real_distribution<>(0, 1)(rng_);
        double cumulative = 0;
        int sampled = 0;
        for (int a = 0; a < num_actions; ++a) {
            cumulative += strategy[a];
            if (r <= cumulative) { sampled = a; break; }
        }

        double new_reach[6];
        std::copy(reach_prob, reach_prob + 6, new_reach);
        new_reach[acting] *= strategy[sampled];

        return cfr_traverse(
            node->children[sampled].get(), traverser,
            hands, board, board_size, new_reach, depth + 1
        );
    }
}

void MCCFRTrainer::train(int iterations) {
    // Build tree if not yet built
    if (!game_tree_) {
        GameTreeBuilder builder(config_.tree_config);
        game_tree_ = builder.build();
    }

    // ─── Open log file ───────────────────────────────────────
    std::ofstream log_fs;
    if (!config_.log_file.empty()) {
        log_fs.open(config_.log_file, std::ios::app);
        if (!log_fs.is_open()) {
            std::cerr << "Warning: could not open log file: " << config_.log_file << std::endl;
        }
    }

    // Helper: write to both stdout and log file
    auto log = [&](const std::string& msg) {
        std::cout << msg << std::endl;
        if (log_fs.is_open()) {
            log_fs << msg << std::endl;
            log_fs.flush();
        }
    };

    // Helper: get current timestamp string
    auto timestamp = []() -> std::string {
        auto now = std::chrono::system_clock::now();
        auto t = std::chrono::system_clock::to_time_t(now);
        char buf[32];
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&t));
        return std::string(buf);
    };

    // ─── Log training header ─────────────────────────────────
    {
        std::ostringstream hdr;
        hdr << "\n══════════════════════════════════════════════════\n"
            << "[" << timestamp() << "] Training started\n"
            << "  Players:        " << config_.num_players << "\n"
            << "  Target iters:   " << config_.iterations << "\n"
            << "  Remaining:      " << iterations << "\n"
            << "  Starting from:  " << total_iterations_ << "\n"
            << "  Checkpoint:     every " << config_.checkpoint_every << "\n"
            << "  Log interval:   every " << config_.log_every << "\n"
            << "  Variant:        " << (config_.variant == CFRVariant::CFR_PLUS ? "CFR+" :
                                        config_.variant == CFRVariant::DCFR ? "DCFR" :
                                        config_.variant == CFRVariant::LINEAR ? "Linear" : "Vanilla") << "\n"
            << "  Flop buckets:   " << config_.abstraction_config.flop_buckets << "\n"
            << "  Turn buckets:   " << config_.abstraction_config.turn_buckets << "\n"
            << "  River buckets:  " << config_.abstraction_config.river_buckets << "\n"
            << "  Bet sizes:      [";
        for (size_t i = 0; i < config_.tree_config.bet_sizes.size(); ++i) {
            if (i > 0) hdr << ", ";
            hdr << config_.tree_config.bet_sizes[i] << "x";
        }
        hdr << "]\n"
            << "══════════════════════════════════════════════════";
        log(hdr.str());
    }

    int log_every = config_.log_every > 0 ? config_.log_every : config_.checkpoint_every;

    auto start = std::chrono::steady_clock::now();

    for (int iter = 0; iter < iterations; ++iter) {
        // Sample a deal
        Card hands[6][2];
        Card board[5];
        sample_deal(hands, board, config_.num_players);

        // Traverse for each player
        for (int p = 0; p < config_.num_players; ++p) {
            double reach[6];
            std::fill(reach, reach + 6, 1.0);
            cfr_traverse(game_tree_.get(), p, hands, board, 5, reach, 0);
        }

        total_iterations_++;

        // ─── DCFR: apply discount factors to all info sets ────────
        if (config_.variant == CFRVariant::DCFR) {
            double t = static_cast<double>(total_iterations_);
            double pos_disc = std::pow(t, config_.dcfr_alpha) /
                              (std::pow(t, config_.dcfr_alpha) + 1.0);
            double neg_disc = std::pow(t, config_.dcfr_beta) /
                              (std::pow(t, config_.dcfr_beta) + 1.0);
            double strat_disc = std::pow(t / (t + 1.0), config_.dcfr_gamma);

            for (auto& [key, data] : info_sets_) {
                for (int a = 0; a < data.num_actions; ++a) {
                    data.regret_sum[a] *= (data.regret_sum[a] > 0)
                                           ? pos_disc : neg_disc;
                    data.strategy_sum[a] *= strat_disc;
                }
            }
        }

        // ─── Progress logging ────────────────────────────────────
        if ((iter + 1) % log_every == 0) {
            auto now = std::chrono::steady_clock::now();
            double elapsed = std::chrono::duration<double>(now - start).count();
            double pct = 100.0 * (iter + 1) / iterations;
            double iters_per_sec = (iter + 1) / elapsed;
            double eta_sec = (iterations - iter - 1) / iters_per_sec;
            int eta_h = static_cast<int>(eta_sec / 3600);
            int eta_m = static_cast<int>((eta_sec - eta_h * 3600) / 60);
            int eta_s = static_cast<int>(eta_sec) % 60;

            // Memory estimate: each info set ~128 bytes + hash overhead
            double mem_mb = info_sets_.size() * 160.0 / (1024 * 1024);

            std::ostringstream msg;
            msg << "[" << timestamp() << "] "
                << "[" << std::fixed << std::setprecision(1) << pct << "%] "
                << "Iter " << total_iterations_
                << " | InfoSets: " << info_sets_.size()
                << " | " << std::setprecision(0) << iters_per_sec << " it/s"
                << " | Elapsed: " << std::setprecision(1) << elapsed << "s"
                << " | ETA: " << eta_h << "h" << eta_m << "m" << eta_s << "s"
                << " | Mem: ~" << std::setprecision(1) << mem_mb << "MB";
            log(msg.str());
        }

        // ─── Checkpoint ──────────────────────────────────────────
        if ((iter + 1) % config_.checkpoint_every == 0 && !config_.checkpoint_dir.empty()) {
            std::string path = config_.checkpoint_dir + "/checkpoint_" +
                              std::to_string(total_iterations_) + ".bin";
            save(path);
            log("[" + timestamp() + "] Checkpoint saved: " + path);
        }
    }

    // ─── Training complete ───────────────────────────────────
    {
        auto end = std::chrono::steady_clock::now();
        double total_sec = std::chrono::duration<double>(end - start).count();
        int h = static_cast<int>(total_sec / 3600);
        int m = static_cast<int>((total_sec - h * 3600) / 60);
        int s = static_cast<int>(total_sec) % 60;

        std::ostringstream msg;
        msg << "[" << timestamp() << "] Training complete!\n"
            << "  Total iterations: " << total_iterations_ << "\n"
            << "  Info sets:        " << info_sets_.size() << "\n"
            << "  Total time:       " << h << "h" << m << "m" << s << "s\n"
            << "  Avg speed:        " << std::fixed << std::setprecision(0)
            << iterations / total_sec << " it/s";
        log(msg.str());
    }
}

void MCCFRTrainer::get_strategy(
    const InfoSetKey& key, double strategy[NUM_ACTIONS]
) const {
    auto it = info_sets_.find(key);
    if (it != info_sets_.end()) {
        it->second.get_average_strategy(strategy);
    } else {
        // Unknown state: uniform over common actions
        std::fill(strategy, strategy + NUM_ACTIONS, 0);
        strategy[static_cast<int>(ActionType::FOLD)] = 0.3;
        strategy[static_cast<int>(ActionType::CALL)] = 0.5;
        strategy[static_cast<int>(ActionType::RAISE_POT)] = 0.2;
    }
}

bool MCCFRTrainer::save(const std::string& path) const {
    std::ofstream ofs(path, std::ios::binary);
    if (!ofs) return false;

    // Version header for forward compatibility
    uint32_t version = 2;
    ofs.write(reinterpret_cast<const char*>(&version), sizeof(version));

    // Write metadata
    int64_t num_sets = info_sets_.size();
    ofs.write(reinterpret_cast<const char*>(&total_iterations_), sizeof(total_iterations_));
    ofs.write(reinterpret_cast<const char*>(&num_sets), sizeof(num_sets));

    // Write each info set
    for (const auto& [key, data] : info_sets_) {
        ofs.write(reinterpret_cast<const char*>(&key), sizeof(InfoSetKey));
        ofs.write(reinterpret_cast<const char*>(&data), sizeof(InfoSetData));
    }

    return true;
}

bool MCCFRTrainer::load(const std::string& path) {
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) return false;

    uint32_t version;
    ifs.read(reinterpret_cast<char*>(&version), sizeof(version));
    if (version != 2) {
        std::cerr << "Incompatible checkpoint version " << version
                  << " (expected 2). Starting fresh." << std::endl;
        return false;
    }

    int64_t num_sets;
    ifs.read(reinterpret_cast<char*>(&total_iterations_), sizeof(total_iterations_));
    ifs.read(reinterpret_cast<char*>(&num_sets), sizeof(num_sets));

    info_sets_.clear();
    info_sets_.reserve(num_sets);

    for (int64_t i = 0; i < num_sets; ++i) {
        InfoSetKey key;
        ifs.read(reinterpret_cast<char*>(&key), sizeof(InfoSetKey));

        InfoSetData data;
        ifs.read(reinterpret_cast<char*>(&data), sizeof(InfoSetData));

        info_sets_[key] = data;
    }

    return true;
}

} // namespace poker
