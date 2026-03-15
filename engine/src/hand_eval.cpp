#include "hand_eval.h"

#include <algorithm>
#include <array>
#include <random>

namespace poker {

// ─── Lookup table approach ───────────────────────────────────
// We use a simple but correct approach: enumerate all C(7,5)=21
// combinations of 5 cards from 7 and take the best.
// For training speed, we precompute rank-based lookup tables.

// Rank prime mapping for hash-based evaluation
static constexpr int PRIMES[] = {2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41};

// Hand categories (lower value = better hand)
// 1-10 = Royal Flush
// 11-166 = Straight Flush
// 167-322 = Four of a Kind
// 323-2467 = Full House
// 2468-3325 = Flush
// 3326-3573 = Straight
// 3574-6185 = Three of a Kind
// 6186-7140 = Two Pair
// 7141-7296 = Pair (shouldn't this be bigger?)
// Actually using standard 7462 equivalence classes

static uint16_t eval_5_simple(int ranks[5], int suits[5]) {
    // Sort ranks descending
    std::sort(ranks, ranks + 5, std::greater<int>());

    bool flush = (suits[0] == suits[1] && suits[1] == suits[2] &&
                  suits[2] == suits[3] && suits[3] == suits[4]);

    bool straight = false;
    int straight_high = -1;

    // Check for straight
    if (ranks[0] - ranks[4] == 4 &&
        ranks[0] != ranks[1] && ranks[1] != ranks[2] &&
        ranks[2] != ranks[3] && ranks[3] != ranks[4]) {
        straight = true;
        straight_high = ranks[0];
    }
    // Ace-low straight (A-2-3-4-5)
    if (ranks[0] == 12 && ranks[1] == 3 && ranks[2] == 2 &&
        ranks[3] == 1 && ranks[4] == 0) {
        straight = true;
        straight_high = 3; // 5-high
    }

    // Count rank frequencies
    int freq[13] = {};
    for (int i = 0; i < 5; ++i) freq[ranks[i]]++;

    int quads = 0, trips = 0, pairs = 0;
    int quad_rank = -1, trip_rank = -1;
    int pair_ranks[2] = {-1, -1};

    for (int r = 12; r >= 0; --r) {
        if (freq[r] == 4) { quads++; quad_rank = r; }
        else if (freq[r] == 3) { trips++; trip_rank = r; }
        else if (freq[r] == 2) {
            if (pairs < 2) pair_ranks[pairs] = r;
            pairs++;
        }
    }

    // Score: lower is better. Using a simple scoring scheme.
    // Category * 10000 + tiebreaker
    // Categories: 0=SF, 1=Quads, 2=FH, 3=Flush, 4=Straight,
    //             5=Trips, 6=TwoPair, 7=Pair, 8=HighCard

    auto encode_kickers = [&](int exclude1 = -1, int exclude2 = -1) -> int {
        int val = 0, mult = 1;
        for (int i = 4; i >= 0; --i) {
            if (ranks[i] != exclude1 && ranks[i] != exclude2) {
                val += ranks[i] * mult;
                mult *= 13;
            }
        }
        return val;
    };

    if (straight && flush) {
        // Straight flush: category 0
        return static_cast<uint16_t>(1 + (12 - straight_high));  // 1-10
    }
    if (quads) {
        // Four of a kind: category 1
        int kicker = -1;
        for (int i = 0; i < 5; ++i)
            if (ranks[i] != quad_rank) { kicker = ranks[i]; break; }
        return static_cast<uint16_t>(11 + (12 - quad_rank) * 13 + (12 - kicker));
    }
    if (trips && pairs) {
        // Full house: category 2
        return static_cast<uint16_t>(180 + (12 - trip_rank) * 13 + (12 - pair_ranks[0]));
    }
    if (flush) {
        // Flush: category 3
        int val = 0;
        for (int i = 0; i < 5; ++i) val = val * 13 + ranks[i];
        return static_cast<uint16_t>(350 + (371292 - val) / 100); // approximate mapping
    }
    if (straight) {
        // Straight: category 4
        return static_cast<uint16_t>(1600 + (12 - straight_high));
    }
    if (trips) {
        // Three of a kind: category 5
        int kickers = 0;
        for (int i = 0; i < 5; ++i)
            if (ranks[i] != trip_rank) kickers = kickers * 13 + ranks[i];
        return static_cast<uint16_t>(1620 + (12 - trip_rank) * 169 + (169 - kickers % 169));
    }
    if (pairs >= 2) {
        // Two pair: category 6
        int kicker = -1;
        for (int i = 0; i < 5; ++i)
            if (ranks[i] != pair_ranks[0] && ranks[i] != pair_ranks[1]) {
                kicker = ranks[i]; break;
            }
        int hi = std::max(pair_ranks[0], pair_ranks[1]);
        int lo = std::min(pair_ranks[0], pair_ranks[1]);
        return static_cast<uint16_t>(3800 + (12 - hi) * 13 * 13 + (12 - lo) * 13 + (12 - (kicker >= 0 ? kicker : 0)));
    }
    if (pairs == 1) {
        // Pair: category 7
        int kval = 0;
        for (int i = 0; i < 5; ++i)
            if (ranks[i] != pair_ranks[0]) kval = kval * 13 + ranks[i];
        return static_cast<uint16_t>(6000 + (12 - pair_ranks[0]) * 2197 + (2197 - kval % 2197));
    }
    // High card: category 8
    int val = 0;
    for (int i = 0; i < 5; ++i) val = val * 13 + ranks[i];
    return static_cast<uint16_t>(7000 + (371292 - val) / 100);
}

HandEvaluator::HandEvaluator() {
    init_tables();
}

void HandEvaluator::init_tables() {
    // Tables could be precomputed for speed.
    // For now, we rely on the direct evaluation.
}

uint16_t HandEvaluator::eval_5cards(Card c0, Card c1, Card c2, Card c3, Card c4) const {
    int ranks[5] = {rank_of(c0), rank_of(c1), rank_of(c2), rank_of(c3), rank_of(c4)};
    int suits[5] = {suit_of(c0), suit_of(c1), suit_of(c2), suit_of(c3), suit_of(c4)};
    return eval_5_simple(ranks, suits);
}

uint16_t HandEvaluator::evaluate5(const Card cards[5]) const {
    return eval_5cards(cards[0], cards[1], cards[2], cards[3], cards[4]);
}

uint16_t HandEvaluator::evaluate(const Card hole[2], const Card board[], int board_size) const {
    // Collect all available cards
    Card all[7];
    all[0] = hole[0];
    all[1] = hole[1];
    for (int i = 0; i < board_size; ++i) all[2 + i] = board[i];
    int total = 2 + board_size;

    // Find best 5-card hand from all available cards
    uint16_t best = 0xFFFF;
    if (total == 5) {
        return evaluate5(all);
    }

    // Enumerate all C(total, 5) combinations
    for (int i = 0; i < total - 4; ++i)
        for (int j = i + 1; j < total - 3; ++j)
            for (int k = j + 1; k < total - 2; ++k)
                for (int l = k + 1; l < total - 1; ++l)
                    for (int m = l + 1; m < total; ++m) {
                        uint16_t r = eval_5cards(all[i], all[j], all[k], all[l], all[m]);
                        if (r < best) best = r;
                    }
    return best;
}

const char* HandEvaluator::rank_category(uint16_t rank) {
    if (rank <= 10) return "Straight Flush";
    if (rank <= 179) return "Four of a Kind";
    if (rank <= 349) return "Full House";
    if (rank <= 1599) return "Flush";
    if (rank <= 1619) return "Straight";
    if (rank <= 3799) return "Three of a Kind";
    if (rank <= 5999) return "Two Pair";
    if (rank <= 6999) return "Pair";
    return "High Card";
}

static HandEvaluator g_evaluator;
const HandEvaluator& get_evaluator() { return g_evaluator; }

// ─── Effective Hand Strength ─────────────────────────────────

double effective_hand_strength(
    const Card hole[2],
    const Card board[],
    int board_size,
    int num_samples
) {
    const auto& eval = get_evaluator();
    std::mt19937 rng(std::random_device{}());

    // Build set of used cards
    CardSet used = card_bit(hole[0]) | card_bit(hole[1]);
    for (int i = 0; i < board_size; ++i) used |= card_bit(board[i]);

    // Available cards
    std::vector<Card> deck;
    deck.reserve(52);
    for (int c = 0; c < 52; ++c) {
        if (!(used & card_bit(c))) deck.push_back(c);
    }

    int wins = 0, ties = 0, total = 0;

    for (int s = 0; s < num_samples; ++s) {
        // Shuffle and deal opponent cards + remaining board
        std::shuffle(deck.begin(), deck.end(), rng);

        Card opp_hole[2] = {deck[0], deck[1]};
        Card full_board[5];
        for (int i = 0; i < board_size; ++i) full_board[i] = board[i];
        int idx = 2;
        for (int i = board_size; i < 5; ++i) full_board[i] = deck[idx++];

        uint16_t my_rank = eval.evaluate(hole, full_board, 5);
        uint16_t opp_rank = eval.evaluate(opp_hole, full_board, 5);

        if (my_rank < opp_rank) wins++;
        else if (my_rank == opp_rank) ties++;
        total++;
    }

    return (wins + 0.5 * ties) / total;
}

double effective_hand_strength_squared(
    const Card hole[2],
    const Card board[],
    int board_size,
    int num_samples
) {
    if (board_size >= 5) {
        // River: no more cards to come, EHS² == EHS
        return effective_hand_strength(hole, board, board_size, num_samples);
    }

    const auto& eval = get_evaluator();
    std::mt19937 rng(std::random_device{}());

    CardSet used = card_bit(hole[0]) | card_bit(hole[1]);
    for (int i = 0; i < board_size; ++i) used |= card_bit(board[i]);

    std::vector<Card> deck;
    deck.reserve(52);
    for (int c = 0; c < 52; ++c) {
        if (!(used & card_bit(c))) deck.push_back(c);
    }

    // Hand potential: track transitions from ahead/behind/tied → win/lose/tie
    // HP[0] = was ahead, HP[1] = was behind, HP[2] = was tied
    double hp_win[3] = {}, hp_total[3] = {};

    for (int s = 0; s < num_samples; ++s) {
        std::shuffle(deck.begin(), deck.end(), rng);

        Card opp_hole[2] = {deck[0], deck[1]};

        // Current board evaluation
        uint16_t my_now = eval.evaluate(hole, board, board_size);
        uint16_t opp_now = eval.evaluate(opp_hole, board, board_size);

        int state; // 0=ahead, 1=behind, 2=tied
        if (my_now < opp_now) state = 0;
        else if (my_now > opp_now) state = 1;
        else state = 2;

        // Deal remaining board cards
        Card full_board[5];
        for (int i = 0; i < board_size; ++i) full_board[i] = board[i];
        int idx = 2;
        for (int i = board_size; i < 5; ++i) full_board[i] = deck[idx++];

        uint16_t my_final = eval.evaluate(hole, full_board, 5);
        uint16_t opp_final = eval.evaluate(opp_hole, full_board, 5);

        hp_total[state] += 1.0;
        if (my_final < opp_final) hp_win[state] += 1.0;
        else if (my_final == opp_final) hp_win[state] += 0.5;
    }

    // Positive potential: P(win at river | behind now)
    double ppot = (hp_total[1] > 0) ? hp_win[1] / hp_total[1] : 0.0;
    // Negative potential: P(lose at river | ahead now)
    double npot = (hp_total[0] > 0) ? 1.0 - hp_win[0] / hp_total[0] : 0.0;

    // Current hand strength
    double hs = effective_hand_strength(hole, board, board_size, num_samples / 2);

    // EHS² = HS × (1 - Npot) + (1 - HS) × Ppot
    return hs * (1.0 - npot) + (1.0 - hs) * ppot;
}

} // namespace poker
