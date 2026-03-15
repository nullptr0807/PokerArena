#pragma once

#include "card.h"
#include <vector>
#include <cstdint>

namespace poker {

// ─── Hand Abstraction ────────────────────────────────────────
// Maps concrete hands to abstract buckets to reduce game tree size.
//
// Strategy:
// - Preflop: 169 canonical hand types (accounts for suit isomorphism)
// - Flop/Turn/River: EHS-based k-means clustering into N buckets

class HandAbstraction {
public:
    // Number of buckets per street
    struct Config {
        int preflop_buckets = 169;  // canonical hand types
        int flop_buckets = 50;
        int turn_buckets = 50;
        int river_buckets = 50;
        int ehs_samples = 100;      // MC samples for EHS calculation
    };

    explicit HandAbstraction(Config config);

    // Get bucket index for a hand on a given street
    int get_bucket(
        Street street,
        const Card hole[2],
        const Card board[],
        int board_size
    ) const;

    // Total buckets for a street
    int num_buckets(Street street) const;

    // Precompute bucket centroids (call once, save to disk)
    void train(int num_iterations = 50);

    // Save/load trained abstraction
    bool save(const std::string& path) const;
    bool load(const std::string& path);

private:
    Config config_;

    // Preflop: canonical hand index (0-168)
    int preflop_bucket(const Card hole[2]) const;

    // Post-flop: EHS-based bucket assignment
    int postflop_bucket(
        Street street,
        const Card hole[2],
        const Card board[],
        int board_size
    ) const;

    // Cluster centroids for post-flop streets [street][bucket] = centroid EHS
    std::vector<std::vector<double>> centroids_[3]; // flop, turn, river
    bool trained_ = false;
};

// ─── Preflop canonical hand types ────────────────────────────
// 13 pairs + 78 suited + 78 offsuit = 169

int canonical_hand_index(Card c1, Card c2);

} // namespace poker
