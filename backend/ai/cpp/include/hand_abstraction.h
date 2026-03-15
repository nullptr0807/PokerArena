#pragma once
/**
 * Hand abstraction using Expected Hand Strength (EHS).
 *
 * Buckets hands into clusters to reduce the game tree size
 * for MCCFR training. Uses Monte Carlo rollouts to estimate
 * equity against random opponent ranges.
 */

#include <array>
#include <cstdint>
#include <vector>

namespace poker {

// 52 cards, 0-51. suit = card/13, rank = card%13 (0=2..12=A)
constexpr int NUM_CARDS = 52;

// Number of buckets per street
constexpr int PREFLOP_BUCKETS = 169;   // canonical hand combos
constexpr int FLOP_BUCKETS    = 50;
constexpr int TURN_BUCKETS    = 50;
constexpr int RIVER_BUCKETS   = 50;

/**
 * Compute Expected Hand Strength via Monte Carlo rollouts.
 *
 * @param hole     2 hole cards
 * @param board    0-5 community cards
 * @param rollouts Number of MC rollouts
 * @return EHS in [0.0, 1.0]
 */
double compute_ehs(
    const std::array<int, 2>& hole,
    const std::vector<int>& board,
    int rollouts = 1000
);

/**
 * Map a hand + board to an abstraction bucket index.
 *
 * @param hole     2 hole cards
 * @param board    community cards (0=preflop, 3=flop, 4=turn, 5=river)
 * @return bucket index
 */
int hand_to_bucket(
    const std::array<int, 2>& hole,
    const std::vector<int>& board
);

} // namespace poker
