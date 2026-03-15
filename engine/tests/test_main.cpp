// Minimal test framework (no external dependency)

#include "card.h"
#include "hand_eval.h"
#include "abstraction.h"
#include "game_tree.h"

#include <cassert>
#include <iostream>
#include <cstring>

#define TEST(name) \
    static void test_##name(); \
    struct Register_##name { Register_##name() { tests.push_back({#name, test_##name}); } } reg_##name; \
    static void test_##name()

struct TestEntry {
    const char* name;
    void (*fn)();
};
static std::vector<TestEntry> tests;

// ─── Card tests ──────────────────────────────────────────────

TEST(card_basics) {
    auto c = poker::make_card(12, 0); // Ace of spades
    assert(poker::rank_of(c) == 12);
    assert(poker::suit_of(c) == 0);
    assert(poker::card_to_string(c) == "As");
    assert(poker::string_to_card("As") == c);
}

TEST(card_roundtrip) {
    for (int i = 0; i < 52; ++i) {
        auto c = static_cast<poker::Card>(i);
        auto s = poker::card_to_string(c);
        assert(poker::string_to_card(s) == c);
    }
}

// ─── Hand evaluator tests ────────────────────────────────────

TEST(eval_straight_flush_beats_quads) {
    const auto& eval = poker::get_evaluator();
    // Straight flush: As Ks Qs Js Ts
    poker::Card sf_hole[2] = {poker::string_to_card("As"), poker::string_to_card("Ks")};
    poker::Card board[5] = {
        poker::string_to_card("Qs"), poker::string_to_card("Js"),
        poker::string_to_card("Ts"), poker::string_to_card("2d"),
        poker::string_to_card("3c")
    };
    uint16_t sf_rank = eval.evaluate(sf_hole, board, 5);

    // Four aces
    poker::Card quads_hole[2] = {poker::string_to_card("Ah"), poker::string_to_card("Ad")};
    poker::Card board2[5] = {
        poker::string_to_card("Ac"), poker::string_to_card("As"),
        poker::string_to_card("Kh"), poker::string_to_card("2d"),
        poker::string_to_card("3c")
    };
    uint16_t quads_rank = eval.evaluate(quads_hole, board2, 5);

    assert(sf_rank < quads_rank); // lower = better
}

TEST(eval_category) {
    assert(std::string(poker::HandEvaluator::rank_category(1)) == "Straight Flush");
    assert(std::string(poker::HandEvaluator::rank_category(100)) == "Four of a Kind");
    assert(std::string(poker::HandEvaluator::rank_category(300)) == "Full House");
}

TEST(effective_hand_strength_range) {
    poker::Card hole[2] = {poker::string_to_card("As"), poker::string_to_card("Ah")};
    poker::Card board[3] = {
        poker::string_to_card("Ks"), poker::string_to_card("7d"),
        poker::string_to_card("2c")
    };
    double ehs = poker::effective_hand_strength(hole, board, 3, 500);
    // AA on this board should be very strong
    assert(ehs > 0.8);
    assert(ehs <= 1.0);
}

// ─── Abstraction tests ──────────────────────────────────────

TEST(canonical_hand_index) {
    // AA
    int aa = poker::canonical_hand_index(
        poker::string_to_card("As"), poker::string_to_card("Ah"));
    assert(aa == 12); // AA is highest pair

    // 22
    int twos = poker::canonical_hand_index(
        poker::string_to_card("2s"), poker::string_to_card("2h"));
    assert(twos == 0);

    // AKs
    int aks = poker::canonical_hand_index(
        poker::string_to_card("As"), poker::string_to_card("Ks"));
    assert(aks >= 13 && aks < 91); // suited range

    // AKo
    int ako = poker::canonical_hand_index(
        poker::string_to_card("As"), poker::string_to_card("Kh"));
    assert(ako >= 91 && ako < 169); // offsuit range

    // Verify uniqueness
    assert(aks != ako);
}

TEST(abstraction_buckets) {
    poker::HandAbstraction::Config abs_cfg; poker::HandAbstraction abs(abs_cfg);
    assert(abs.num_buckets(poker::Street::PREFLOP) == 169);
    assert(abs.num_buckets(poker::Street::FLOP) == 50);

    poker::Card hole[2] = {poker::string_to_card("As"), poker::string_to_card("Ah")};
    poker::Card board[3] = {
        poker::string_to_card("Ks"), poker::string_to_card("7d"),
        poker::string_to_card("2c")
    };

    int bucket_pre = abs.get_bucket(poker::Street::PREFLOP, hole, nullptr, 0);
    assert(bucket_pre >= 0 && bucket_pre < 169);

    int bucket_flop = abs.get_bucket(poker::Street::FLOP, hole, board, 3);
    assert(bucket_flop >= 0 && bucket_flop < 50);
}

// ─── Game tree tests ─────────────────────────────────────────

TEST(game_tree_builds) {
    poker::GameTreeBuilder::Config cfg;
    cfg.num_players = 2;
    cfg.starting_stack = 200;
    cfg.max_raises_per_street = 1;
    cfg.bet_sizes = {0.75};
    poker::GameTreeBuilder builder(cfg);
    auto tree = builder.build();
    assert(tree != nullptr);
    assert(tree->type == poker::NodeType::PLAYER);
    assert(!tree->valid_actions.empty());
}

TEST(game_tree_3player) {
    poker::GameTreeBuilder::Config cfg;
    cfg.num_players = 3;
    cfg.starting_stack = 200;
    cfg.max_raises_per_street = 1;
    cfg.bet_sizes = {0.75};
    poker::GameTreeBuilder builder(cfg);
    auto tree = builder.build();
    assert(tree != nullptr);
    assert(tree->num_players == 3);
}

// ─── Main ────────────────────────────────────────────────────

int main() {
    int passed = 0, failed = 0;
    for (const auto& t : tests) {
        try {
            t.fn();
            std::cout << "  PASS: " << t.name << std::endl;
            passed++;
        } catch (const std::exception& e) {
            std::cout << "  FAIL: " << t.name << " - " << e.what() << std::endl;
            failed++;
        } catch (...) {
            std::cout << "  FAIL: " << t.name << " - unknown error" << std::endl;
            failed++;
        }
    }
    std::cout << "\n" << passed << " passed, " << failed << " failed" << std::endl;
    return failed > 0 ? 1 : 0;
}
