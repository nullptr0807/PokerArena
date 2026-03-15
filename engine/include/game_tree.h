#pragma once

#include "card.h"
#include <cstdint>
#include <memory>
#include <vector>
#include <string>

namespace poker {

// ─── Game Tree ───────────────────────────────────────────────
// Abstract game tree for CFR training.
// Each node is either:
//   - A chance node (deal cards)
//   - A player action node (fold/check/call/raise/all_in)
//   - A terminal node (showdown or fold)

enum class NodeType : uint8_t {
    PLAYER,    // player decision point
    TERMINAL,  // hand is over
};

struct GameNode {
    NodeType type;
    int player;          // acting player (for PLAYER nodes)
    Street street;
    int pot;             // current pot size (in BBs * 100 for precision)
    int stacks[6];       // remaining stack per player
    int bets[6];         // current bet per player this street
    int num_players;     // total players at table
    bool folded[6];      // who has folded
    bool all_in[6];      // who is all-in
    int current_bet;     // highest bet this street
    int last_raiser;     // last player who raised (-1 if none)
    int num_raises;      // raises this street (for cap)

    // For terminal nodes
    double payoffs[6];   // payoff per player (in BBs)

    // Valid actions at this node
    std::vector<ActionType> valid_actions;

    // Children indexed by action
    std::vector<std::unique_ptr<GameNode>> children;

    GameNode() = default;
};

// ─── Game Tree Builder ───────────────────────────────────────

class GameTreeBuilder {
public:
    struct Config {
        int num_players = 2;
        int starting_stack = 200;  // in BB
        int small_blind = 1;       // in BB (using 1 = 0.5BB for SB)
        int big_blind = 2;         // in BB (using 2 = 1BB)
        int max_raises_per_street = 4;

        // Bet sizing abstractions (fractions of pot)
        std::vector<double> bet_sizes = {0.25, 0.5, 0.75, 1.0, 1.5};
    };

    explicit GameTreeBuilder(Config config);

    // Build the full abstract game tree
    std::unique_ptr<GameNode> build();

    const Config& config() const { return config_; }

private:
    Config config_;

    void build_recursive(
        GameNode* node,
        Street street,
        int acting_player,
        int pot,
        int stacks[],
        int bets[],
        bool folded[],
        bool all_in[],
        int current_bet,
        int last_raiser,
        int num_raises,
        int num_players,
        int button,
        int close_player = -1
    );

    std::vector<ActionType> get_valid_actions(
        int player,
        int stack,
        int current_bet,
        int player_bet,
        int pot,
        int num_raises,
        int max_raises
    ) const;

    int next_active_player(
        int from,
        const bool folded[],
        const bool all_in[],
        int num_players
    ) const;

    int count_active(const bool folded[], const bool all_in[], int n) const;
    int count_in_hand(const bool folded[], int n) const;
};

} // namespace poker
