/**
 * Hand abstraction — EHS computation via Monte Carlo rollouts.
 */

#include "hand_abstraction.h"
#include <algorithm>
#include <array>
#include <random>
#include <bitset>

namespace poker {

namespace {

// 7-card hand evaluator (simplified rank-based)
// Returns a comparable score — lower is better (1=best)
// Uses a basic lookup approach for speed

struct HandRank {
    int category; // 0=high card..8=straight flush
    int tiebreak; // secondary rank for comparison
};

constexpr int rank_of(int card) { return card % 13; }  // 0=2..12=A
constexpr int suit_of(int card) { return card / 13; }

// Count occurrences of each rank in a set of cards
std::array<int, 13> rank_counts(const int* cards, int n) {
    std::array<int, 13> counts{};
    for (int i = 0; i < n; ++i) counts[rank_of(cards[i])]++;
    return counts;
}

// Check for flush (5+ cards of same suit) among 7 cards
int flush_suit(const int* cards, int n) {
    std::array<int, 4> suit_counts{};
    for (int i = 0; i < n; ++i) suit_counts[suit_of(cards[i])]++;
    for (int s = 0; s < 4; ++s) {
        if (suit_counts[s] >= 5) return s;
    }
    return -1;
}

// Check for straight — returns highest card of straight, or -1
int check_straight(const std::array<int, 13>& counts) {
    // Check A-high down to 5-high (wheel)
    // Wheel: A-2-3-4-5 → ranks 12,0,1,2,3
    int consecutive = 0;
    for (int r = 12; r >= 0; --r) {
        if (counts[r] > 0) {
            consecutive++;
            if (consecutive >= 5) return r + 4; // highest card of straight
        } else {
            consecutive = 0;
        }
    }
    // Check wheel: A,2,3,4,5
    if (counts[12] > 0 && counts[0] > 0 && counts[1] > 0 && counts[2] > 0 && counts[3] > 0) {
        return 3; // 5-high straight
    }
    return -1;
}

// Simple 7-card hand evaluation → score (lower = better)
int evaluate_7(const int* cards) {
    auto counts = rank_counts(cards, 7);
    int fs = flush_suit(cards, 7);

    // Gather rank info
    std::array<int, 5> quads{}, trips{}, pairs{}, singles{};
    int nq = 0, nt = 0, np = 0, ns = 0;
    for (int r = 12; r >= 0; --r) {
        if (counts[r] == 4) quads[nq++] = r;
        else if (counts[r] == 3) trips[nt++] = r;
        else if (counts[r] == 2) pairs[np++] = r;
        else if (counts[r] == 1) singles[ns++] = r;
    }

    int straight = check_straight(counts);

    // Straight flush check
    if (fs >= 0 && straight >= 0) {
        // Verify the straight is in the flush suit
        std::array<int, 13> flush_ranks{};
        for (int i = 0; i < 7; ++i) {
            if (suit_of(cards[i]) == fs) flush_ranks[rank_of(cards[i])]++;
        }
        int sf = check_straight(flush_ranks);
        if (sf >= 0) return 10 + (12 - sf); // 10=royal flush, higher=worse
    }

    // Four of a kind
    if (nq > 0) return 100 + (12 - quads[0]);

    // Full house
    if (nt > 0 && (np > 0 || nt > 1))
        return 200 + (12 - trips[0]);

    // Flush
    if (fs >= 0) return 300;

    // Straight
    if (straight >= 0) return 400 + (12 - straight);

    // Three of a kind
    if (nt > 0) return 500 + (12 - trips[0]);

    // Two pair
    if (np >= 2) return 600 + (12 - pairs[0]) * 13 + (12 - pairs[1]);

    // One pair
    if (np == 1) return 800 + (12 - pairs[0]);

    // High card
    return 1000 + (12 - singles[0]);
}

thread_local std::mt19937 rng{std::random_device{}()};

} // anonymous namespace


int preflop_bucket(const std::array<int, 2>& hole) {
    int r0 = rank_of(hole[0]), r1 = rank_of(hole[1]);
    int s0 = suit_of(hole[0]), s1 = suit_of(hole[1]);
    int hi = std::max(r0, r1), lo = std::min(r0, r1);
    if (r0 == r1) {
        return hi; // 0-12: pairs (22=0, AA=12)
    } else if (s0 == s1) {
        return 13 + hi * (hi - 1) / 2 + lo;  // 13..90 (78 suited combos)
    } else {
        return 91 + hi * (hi - 1) / 2 + lo;  // 91..168 (78 offsuit combos)
    }
}

double compute_ehs(
    const std::array<int, 2>& hole,
    const std::vector<int>& board,
    int rollouts
) {
    // Build a deck minus known cards
    std::bitset<52> used;
    used.set(hole[0]);
    used.set(hole[1]);
    for (int c : board) used.set(c);

    std::vector<int> deck;
    deck.reserve(52 - used.count());
    for (int i = 0; i < 52; ++i) {
        if (!used.test(i)) deck.push_back(i);
    }

    int wins = 0, ties = 0, total = 0;
    int cards_needed = 5 - static_cast<int>(board.size()); // remaining community
    // opponent has 2 hole cards → need cards_needed + 2 from deck

    for (int r = 0; r < rollouts; ++r) {
        // Partial shuffle: only pick cards_needed + 2 cards from deck
        int need = cards_needed + 2;
        int deck_size = static_cast<int>(deck.size());
        if (deck_size < need) break;
        for (int i = 0; i < need; ++i) {
            std::uniform_int_distribution<int> dist(i, deck_size - 1);
            std::swap(deck[i], deck[dist(rng)]);
        }

        // Deal remaining board
        int full_board[5];
        int bi = 0;
        for (int c : board) full_board[bi++] = c;
        for (int i = 0; i < cards_needed; ++i) full_board[bi++] = deck[i];

        // Opponent hole cards
        int opp0 = deck[cards_needed];
        int opp1 = deck[cards_needed + 1];

        // Evaluate
        int my_cards[7] = {hole[0], hole[1], full_board[0], full_board[1],
                           full_board[2], full_board[3], full_board[4]};
        int opp_cards[7] = {opp0, opp1, full_board[0], full_board[1],
                            full_board[2], full_board[3], full_board[4]};

        int my_score = evaluate_7(my_cards);
        int opp_score = evaluate_7(opp_cards);

        if (my_score < opp_score) wins++;
        else if (my_score == opp_score) ties++;
        total++;
    }

    if (total == 0) return 0.5;
    return (wins + 0.5 * ties) / total;
}


int hand_to_bucket(
    const std::array<int, 2>& hole,
    const std::vector<int>& board
) {
    int num_buckets;
    if (board.empty()) {
        return preflop_bucket(hole);
    } else if (board.size() == 3) {
        num_buckets = FLOP_BUCKETS;
    } else if (board.size() == 4) {
        num_buckets = TURN_BUCKETS;
    } else {
        num_buckets = RIVER_BUCKETS;
    }

    double ehs = compute_ehs(hole, board, 10);
    int bucket = static_cast<int>(ehs * (num_buckets - 1));
    return std::clamp(bucket, 0, num_buckets - 1);
}

} // namespace poker
