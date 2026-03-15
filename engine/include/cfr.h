#pragma once

#include "card.h"
#include "abstraction.h"
#include "game_tree.h"
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include <random>

namespace poker {

// ─── CFR Variant ─────────────────────────────────────────────

enum class CFRVariant {
    VANILLA,    // Standard MCCFR
    CFR_PLUS,   // CFR+: floor negative regrets to 0 each iteration
    LINEAR,     // Linear CFR: weight by iteration t
    DCFR,       // Discounted CFR: configurable discount factors
};

// ─── Information Set ─────────────────────────────────────────
// An information set is what a player knows: their bucket + action history

struct InfoSetKey {
    int bucket;                      // hand abstraction bucket
    std::vector<uint8_t> history;    // action sequence encoding

    bool operator==(const InfoSetKey& other) const {
        return bucket == other.bucket && history == other.history;
    }
};

struct InfoSetKeyHash {
    size_t operator()(const InfoSetKey& k) const {
        size_t h = std::hash<int>{}(k.bucket);
        for (auto a : k.history) {
            h ^= std::hash<uint8_t>{}(a) + 0x9e3779b9 + (h << 6) + (h >> 2);
        }
        return h;
    }
};

// ─── Information Set Data ────────────────────────────────────

struct InfoSetData {
    double regret_sum[NUM_ACTIONS] = {};     // cumulative regret
    double strategy_sum[NUM_ACTIONS] = {};   // cumulative strategy (for averaging)
    int num_actions = 0;                     // number of valid actions

    // Get current strategy via regret matching
    void get_strategy(double strategy[NUM_ACTIONS]) const;

    // Get average strategy (converged Nash equilibrium approximation)
    void get_average_strategy(double strategy[NUM_ACTIONS]) const;
};

// ─── Monte Carlo CFR Trainer ─────────────────────────────────
// External Sampling MCCFR for multi-player poker

class MCCFRTrainer {
public:
    struct Config {
        int num_players = 6;
        int iterations = 1000000;
        int checkpoint_every = 100000;
        std::string checkpoint_dir = "data/";
        CFRVariant variant = CFRVariant::CFR_PLUS;  // default to CFR+
        // DCFR parameters (only used when variant == DCFR)
        double dcfr_alpha = 1.5;   // positive regret discount: t^α / (t^α + 1)
        double dcfr_beta  = 0.0;   // negative regret discount: t^β / (t^β + 1)
        double dcfr_gamma = 2.0;   // strategy sum discount:    (t / (t+1))^γ
        HandAbstraction::Config abstraction_config;
        GameTreeBuilder::Config tree_config;
    };

    explicit MCCFRTrainer(Config config);

    // Load a pre-trained abstraction (must call before train())
    bool load_abstraction(const std::string& path);

    // Run training for N iterations
    void train(int iterations);

    // Get strategy for an information set
    void get_strategy(
        const InfoSetKey& key,
        double strategy[NUM_ACTIONS]
    ) const;

    // Save/load trained strategy
    bool save(const std::string& path) const;
    bool load(const std::string& path);

    // Stats
    int num_info_sets() const { return info_sets_.size(); }
    int64_t total_iterations() const { return total_iterations_; }

private:
    Config config_;
    HandAbstraction abstraction_;
    std::unique_ptr<GameNode> game_tree_;

    // Information set storage
    std::unordered_map<InfoSetKey, InfoSetData, InfoSetKeyHash> info_sets_;
    int64_t total_iterations_ = 0;

    std::mt19937 rng_;

    // External sampling MCCFR
    // Returns utility for the traversing player
    double cfr_traverse(
        GameNode* node,
        int traverser,           // player whose regrets we update
        const Card hands[][2],   // each player's hole cards
        const Card board[],
        int board_size,
        double reach_prob[],     // reach probabilities for each player
        int depth
    );

    // Sample a random deal
    void sample_deal(Card hands[][2], Card board[5], int num_players);

    // Build info set key for current state
    InfoSetKey make_key(
        int player,
        const Card hole[2],
        const Card board[],
        int board_size,
        const GameNode* node
    ) const;
};

} // namespace poker
