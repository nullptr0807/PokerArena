#include "abstraction.h"
#include "hand_eval.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <random>
#include <iostream>

namespace poker {

// ─── Canonical preflop hand index ────────────────────────────
// 169 types: 13 pairs (AA..22) + 78 suited (AKs..32s) + 78 offsuit (AKo..32o)

int canonical_hand_index(Card c1, Card c2) {
    int r1 = rank_of(c1), r2 = rank_of(c2);
    int s1 = suit_of(c1), s2 = suit_of(c2);

    if (r1 < r2) { std::swap(r1, r2); std::swap(s1, s2); }

    bool suited = (s1 == s2);
    bool pair = (r1 == r2);

    if (pair) {
        // Pairs: index 0-12 (AA=12, KK=11, ..., 22=0)
        return r1;
    }
    if (suited) {
        // Suited: 13 + triangular index
        // r1 > r2, enumerate (12,11), (12,10), ..., (1,0)
        int idx = 0;
        for (int i = 12; i > r1; --i)
            idx += i;  // number of combos with higher card = i
        idx += (r1 - 1 - r2);
        return 13 + idx;
    }
    // Offsuit: 91 + same triangular index
    int idx = 0;
    for (int i = 12; i > r1; --i)
        idx += i;
    idx += (r1 - 1 - r2);
    return 91 + idx;
}

HandAbstraction::HandAbstraction(Config config) : config_(config) {}

int HandAbstraction::num_buckets(Street street) const {
    switch (street) {
        case Street::PREFLOP: return config_.preflop_buckets;
        case Street::FLOP:    return config_.flop_buckets;
        case Street::TURN:    return config_.turn_buckets;
        case Street::RIVER:   return config_.river_buckets;
    }
    return 0;
}

int HandAbstraction::get_bucket(
    Street street, const Card hole[2],
    const Card board[], int board_size
) const {
    if (street == Street::PREFLOP) {
        return preflop_bucket(hole);
    }
    return postflop_bucket(street, hole, board, board_size);
}

int HandAbstraction::preflop_bucket(const Card hole[2]) const {
    return canonical_hand_index(hole[0], hole[1]);
}

int HandAbstraction::postflop_bucket(
    Street street, const Card hole[2],
    const Card board[], int board_size
) const {
    int n_buckets = num_buckets(street);

    if (!trained_) {
        // Fast deterministic bucketing: use hand rank as proxy for EHS
        // This avoids expensive MC sampling during CFR training
        const auto& eval = get_evaluator();
        uint16_t rank = eval.evaluate(hole, board, board_size);
        // rank range is roughly 1-7500, lower = better
        // Map to bucket: invert so better hands get higher buckets
        int bucket = (7500 - std::min(static_cast<int>(rank), 7500)) * n_buckets / 7500;
        return std::clamp(bucket, 0, n_buckets - 1);
    }

    // Find nearest centroid using EHS
    double ehs = effective_hand_strength(hole, board, board_size, config_.ehs_samples);

    int street_idx = static_cast<int>(street) - 1; // flop=0, turn=1, river=2
    if (street_idx < 0 || street_idx >= 3) return 0;

    const auto& cents = centroids_[street_idx];
    if (cents.empty()) {
        int bucket = static_cast<int>(ehs * n_buckets);
        return std::clamp(bucket, 0, n_buckets - 1);
    }

    int best = 0;
    double best_dist = 1e9;
    // centroids_[street_idx] is a vector<vector<double>>
    // Each centroid is a single EHS value for simplicity
    for (int i = 0; i < static_cast<int>(cents.size()); ++i) {
        double d = std::abs(ehs - cents[i][0]);
        if (d < best_dist) {
            best_dist = d;
            best = i;
        }
    }
    return best;
}

void HandAbstraction::train(int num_iterations) {
    std::mt19937 rng(42);
    const auto& eval = get_evaluator();

    // For each post-flop street, sample hands and cluster by EHS
    for (int street_idx = 0; street_idx < 3; ++street_idx) {
        Street street = static_cast<Street>(street_idx + 1);
        int n_buckets = num_buckets(street);
        int board_size = (street_idx == 0) ? 3 : (street_idx == 1) ? 4 : 5;

        std::cout << "Training abstraction for "
                  << (street_idx == 0 ? "flop" : street_idx == 1 ? "turn" : "river")
                  << " (" << n_buckets << " buckets)..." << std::endl;

        // Sample EHS values
        std::vector<double> samples;
        int num_samples = n_buckets * 20;  // sample enough for clustering

        std::vector<Card> deck(52);
        for (int i = 0; i < 52; ++i) deck[i] = static_cast<Card>(i);

        for (int s = 0; s < num_samples; ++s) {
            std::shuffle(deck.begin(), deck.end(), rng);
            Card hole[2] = {deck[0], deck[1]};
            Card board[5];
            for (int i = 0; i < board_size; ++i) board[i] = deck[2 + i];

            double ehs = effective_hand_strength(hole, board, board_size, config_.ehs_samples);
            samples.push_back(ehs);
        }

        // K-means clustering
        std::vector<std::vector<double>> centroids(n_buckets, std::vector<double>(1));
        // Initialize centroids uniformly
        for (int i = 0; i < n_buckets; ++i) {
            centroids[i][0] = (i + 0.5) / n_buckets;
        }

        for (int iter = 0; iter < num_iterations; ++iter) {
            // Assignment step
            std::vector<double> sums(n_buckets, 0);
            std::vector<int> counts(n_buckets, 0);

            for (double ehs : samples) {
                int best = 0;
                double best_d = 1e9;
                for (int k = 0; k < n_buckets; ++k) {
                    double d = std::abs(ehs - centroids[k][0]);
                    if (d < best_d) { best_d = d; best = k; }
                }
                sums[best] += ehs;
                counts[best]++;
            }

            // Update step
            for (int k = 0; k < n_buckets; ++k) {
                if (counts[k] > 0) {
                    centroids[k][0] = sums[k] / counts[k];
                }
            }
        }

        // Sort centroids
        std::sort(centroids.begin(), centroids.end());
        centroids_[street_idx] = centroids;
    }

    trained_ = true;
    std::cout << "Abstraction training complete." << std::endl;
}

bool HandAbstraction::save(const std::string& path) const {
    std::ofstream ofs(path, std::ios::binary);
    if (!ofs) return false;

    // Write config
    ofs.write(reinterpret_cast<const char*>(&config_), sizeof(config_));
    ofs.write(reinterpret_cast<const char*>(&trained_), sizeof(trained_));

    // Write centroids
    for (int s = 0; s < 3; ++s) {
        int n = static_cast<int>(centroids_[s].size());
        ofs.write(reinterpret_cast<const char*>(&n), sizeof(n));
        for (const auto& c : centroids_[s]) {
            double val = c[0];
            ofs.write(reinterpret_cast<const char*>(&val), sizeof(val));
        }
    }

    return true;
}

bool HandAbstraction::load(const std::string& path) {
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) return false;

    ifs.read(reinterpret_cast<char*>(&config_), sizeof(config_));
    ifs.read(reinterpret_cast<char*>(&trained_), sizeof(trained_));

    for (int s = 0; s < 3; ++s) {
        int n = 0;
        ifs.read(reinterpret_cast<char*>(&n), sizeof(n));
        centroids_[s].resize(n, std::vector<double>(1));
        for (int i = 0; i < n; ++i) {
            double val;
            ifs.read(reinterpret_cast<char*>(&val), sizeof(val));
            centroids_[s][i][0] = val;
        }
    }

    return true;
}

} // namespace poker
