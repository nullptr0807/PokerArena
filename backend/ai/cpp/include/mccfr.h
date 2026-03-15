#pragma once
/**
 * Monte Carlo Counterfactual Regret Minimization (MCCFR).
 *
 * Implements External Sampling MCCFR for computing approximate
 * Nash equilibrium strategies in NLHE abstractions.
 */

#include <array>
#include <cstdint>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace poker {

// Bet abstraction: available bet sizes as fractions of the pot
constexpr std::array<double, 5> BET_SIZES = {0.3, 0.5, 0.7, 1.0, 1.2};
constexpr int NUM_ACTIONS = 8; // fold, check, call, bet_30, bet_50, bet_70, bet_pot, bet_120, allin

/**
 * Information set node in the game tree.
 * Stores cumulative regrets and strategy sums for MCCFR.
 */
struct InfoNode {
    std::array<double, NUM_ACTIONS> regret_sum{};
    std::array<double, NUM_ACTIONS> strategy_sum{};

    /** Get current strategy via regret matching. */
    std::array<double, NUM_ACTIONS> get_strategy() const;

    /** Get average strategy (converged blueprint). */
    std::array<double, NUM_ACTIONS> get_average_strategy() const;
};

/**
 * MCCFR Trainer.
 *
 * Trains a blueprint strategy over an abstracted NLHE game tree.
 * Uses external sampling for variance reduction.
 */
class MCCFRTrainer {
public:
    MCCFRTrainer(int num_players = 2);

    /**
     * Run MCCFR training iterations.
     * @param iterations Number of iterations (more = better convergence)
     */
    void train(int64_t iterations);
    void train_parallel(int64_t iterations, int num_threads);

    /**
     * Query the blueprint strategy for a given information set.
     * @param info_set_key Canonical string key for the information set
     * @return Action probabilities
     */
    std::array<double, NUM_ACTIONS> query(const std::string& info_set_key) const;

    /** Query by uint64 key (internal). */
    std::array<double, NUM_ACTIONS> query_u64(uint64_t key) const;

    /**
     * Save trained strategy to disk.
     */
    void save(const std::string& path) const;

    /**
     * Load trained strategy from disk.
     */
    void load(const std::string& path);

    /** Number of unique info sets discovered. */
    size_t num_info_sets() const { return nodes_.size(); }

private:
    int num_players_;
    std::unordered_map<uint64_t, InfoNode> nodes_;
    mutable std::shared_mutex nodes_mtx_;  // for parallel training

    /**
     * External sampling MCCFR traversal (internal).
     */
    double cfr_impl(int traverser, int64_t iteration = 0);
};

} // namespace poker
