#pragma once

#include "card.h"
#include <cstdint>

namespace poker {

// Fast hand evaluation using lookup tables.
// Returns a hand rank where LOWER is BETTER (1 = best possible).

class HandEvaluator {
public:
    HandEvaluator();

    // Evaluate best 5-card hand from 7 cards (2 hole + 5 board)
    uint16_t evaluate(const Card hole[2], const Card board[], int board_size) const;

    // Evaluate 5 cards directly
    uint16_t evaluate5(const Card cards[5]) const;

    // Get hand category string
    static const char* rank_category(uint16_t rank);

private:
    // Lookup tables for fast evaluation
    // Using a simplified rank-based approach
    uint16_t flush_table_[8192];     // 2^13 for flush hands
    uint16_t non_flush_table_[8192]; // hash-based for non-flush

    void init_tables();
    uint16_t eval_5cards(Card c0, Card c1, Card c2, Card c3, Card c4) const;
};

// Global evaluator instance
const HandEvaluator& get_evaluator();

// ─── Effective Hand Strength ─────────────────────────────────
// Used for hand abstraction / bucketing

// Calculate EHS (expected hand strength) via Monte Carlo sampling
// Returns value in [0, 1] where 1 = always wins
double effective_hand_strength(
    const Card hole[2],
    const Card board[],
    int board_size,
    int num_samples = 1000
);

// Calculate EHS² (EHS squared, considering positive/negative potential)
double effective_hand_strength_squared(
    const Card hole[2],
    const Card board[],
    int board_size,
    int num_samples = 1000
);

} // namespace poker
