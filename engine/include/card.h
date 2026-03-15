#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace poker {

// Card representation: 0-51
// rank = card % 13  (0=2, 1=3, ..., 12=A)
// suit = card / 13  (0=spade, 1=heart, 2=diamond, 3=club)
using Card = uint8_t;

constexpr int NUM_CARDS = 52;
constexpr int NUM_RANKS = 13;
constexpr int NUM_SUITS = 4;

constexpr char RANK_CHARS[] = "23456789TJQKA";
constexpr char SUIT_CHARS[] = "shdc";

inline int rank_of(Card c) { return c % 13; }
inline int suit_of(Card c) { return c / 13; }

inline Card make_card(int rank, int suit) {
    return static_cast<Card>(suit * 13 + rank);
}

inline std::string card_to_string(Card c) {
    return {RANK_CHARS[rank_of(c)], SUIT_CHARS[suit_of(c)]};
}

inline Card string_to_card(const std::string& s) {
    int rank = -1, suit = -1;
    for (int i = 0; i < NUM_RANKS; ++i)
        if (RANK_CHARS[i] == s[0]) { rank = i; break; }
    for (int i = 0; i < NUM_SUITS; ++i)
        if (SUIT_CHARS[i] == s[1]) { suit = i; break; }
    return make_card(rank, suit);
}

// ─── Hand representation ─────────────────────────────────────

struct HoleCards {
    Card cards[2];
};

// ─── Bit-based card set for fast operations ──────────────────

using CardSet = uint64_t;

inline CardSet card_bit(Card c) { return 1ULL << c; }

inline CardSet make_card_set(const std::vector<Card>& cards) {
    CardSet cs = 0;
    for (auto c : cards) cs |= card_bit(c);
    return cs;
}

inline int popcount(CardSet cs) { return __builtin_popcountll(cs); }

// ─── Action types ────────────────────────────────────────────

enum class ActionType : uint8_t {
    FOLD = 0,
    CHECK = 1,
    CALL = 2,
    RAISE_HALF = 3,   // 0.33x pot
    RAISE_POT = 4,    // 0.75x pot
    RAISE_2X = 5,     // 1.5x pot
    ALL_IN = 6,
    NUM_ACTIONS = 7,
};

constexpr int NUM_ACTIONS = static_cast<int>(ActionType::NUM_ACTIONS);

// Bet sizing multipliers (relative to pot)
constexpr double BET_SIZES[] = {
    0.0,   // FOLD
    0.0,   // CHECK
    0.0,   // CALL
    0.33,  // RAISE_HALF
    0.75,  // RAISE_POT
    1.5,   // RAISE_2X
    0.0,   // ALL_IN (special)
};

inline const char* action_name(ActionType a) {
    switch (a) {
        case ActionType::FOLD: return "fold";
        case ActionType::CHECK: return "check";
        case ActionType::CALL: return "call";
        case ActionType::RAISE_HALF: return "raise_33";
        case ActionType::RAISE_POT: return "raise_75";
        case ActionType::RAISE_2X: return "raise_150";
        case ActionType::ALL_IN: return "all_in";
        default: return "unknown";
    }
}

// ─── Game state ──────────────────────────────────────────────

enum class Street : uint8_t {
    PREFLOP = 0,
    FLOP = 1,
    TURN = 2,
    RIVER = 3,
};

constexpr int NUM_STREETS = 4;

} // namespace poker
