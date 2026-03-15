#include "game_tree.h"

#include <algorithm>

namespace poker {

GameTreeBuilder::GameTreeBuilder(Config config) : config_(config) {}

int GameTreeBuilder::next_active_player(
    int from, const bool folded[], const bool all_in[], int num_players
) const {
    for (int i = 1; i <= num_players; ++i) {
        int idx = (from + i) % num_players;
        if (!folded[idx] && !all_in[idx]) return idx;
    }
    return -1;
}

int GameTreeBuilder::count_active(const bool folded[], const bool all_in[], int n) const {
    int c = 0;
    for (int i = 0; i < n; ++i)
        if (!folded[i] && !all_in[i]) c++;
    return c;
}

int GameTreeBuilder::count_in_hand(const bool folded[], int n) const {
    int c = 0;
    for (int i = 0; i < n; ++i)
        if (!folded[i]) c++;
    return c;
}

std::vector<ActionType> GameTreeBuilder::get_valid_actions(
    int player, int stack, int current_bet, int player_bet,
    int pot, int num_raises, int max_raises
) const {
    std::vector<ActionType> actions;
    int to_call = current_bet - player_bet;

    actions.push_back(ActionType::FOLD);

    if (to_call <= 0) {
        actions.push_back(ActionType::CHECK);
    } else {
        if (to_call >= stack) {
            // Can only fold or go all-in
            actions.push_back(ActionType::ALL_IN);
            return actions;
        }
        actions.push_back(ActionType::CALL);
    }

    // Raise actions (if allowed and affordable)
    if (num_raises < max_raises && stack > to_call) {
        for (double size : config_.bet_sizes) {
            int raise_amount = static_cast<int>(pot * size);
            int raise_to = current_bet + std::max(raise_amount, config_.big_blind);
            int cost = raise_to - player_bet;
            if (cost > 0 && cost < stack) {
                // Map to nearest abstract action
                if (size <= 0.5) actions.push_back(ActionType::RAISE_HALF);
                else if (size <= 1.0) actions.push_back(ActionType::RAISE_POT);
                else actions.push_back(ActionType::RAISE_2X);
            }
        }
        actions.push_back(ActionType::ALL_IN);
    }

    // Deduplicate
    std::sort(actions.begin(), actions.end());
    actions.erase(std::unique(actions.begin(), actions.end()), actions.end());

    return actions;
}

std::unique_ptr<GameNode> GameTreeBuilder::build() {
    auto root = std::make_unique<GameNode>();

    int stacks[6] = {};
    int bets[6] = {};
    bool folded[6] = {};
    bool all_in[6] = {};

    for (int i = 0; i < config_.num_players; ++i) {
        stacks[i] = config_.starting_stack;
    }

    // Post blinds (assuming button=0, SB=1, BB=2 for 3+ players)
    // For heads-up: SB=button=0, BB=1
    int sb_idx, bb_idx, first_to_act;
    if (config_.num_players == 2) {
        sb_idx = 0;
        bb_idx = 1;
        first_to_act = 0; // SB acts first preflop in HU
    } else {
        sb_idx = 1;
        bb_idx = 2;
        first_to_act = 3 % config_.num_players;
    }

    stacks[sb_idx] -= config_.small_blind;
    bets[sb_idx] = config_.small_blind;
    stacks[bb_idx] -= config_.big_blind;
    bets[bb_idx] = config_.big_blind;

    int pot = config_.small_blind + config_.big_blind;

    build_recursive(
        root.get(), Street::PREFLOP, first_to_act,
        pot, stacks, bets, folded, all_in,
        config_.big_blind, -1, 0,
        config_.num_players, 0, bb_idx
    );

    return root;
}

void GameTreeBuilder::build_recursive(
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
    int close_player  // player who closes the action (-1 = use first-to-act)
) {
    // Safety: depth limit to prevent stack overflow
    static thread_local int depth = 0;
    struct DepthGuard { DepthGuard() { ++depth; } ~DepthGuard() { --depth; } } dg;
    if (depth > 200) {
        node->type = NodeType::TERMINAL;
        node->street = street;
        node->pot = pot;
        return;
    }
    // Terminal: only one player left
    if (count_in_hand(folded, num_players) <= 1) {
        node->type = NodeType::TERMINAL;
        node->street = street;
        node->pot = pot;
        // Payoffs computed at query time with actual cards
        return;
    }

    // No active players (all folded or all-in): terminal
    if (count_active(folded, all_in, num_players) == 0) {
        node->type = NodeType::TERMINAL;
        node->street = street;
        node->pot = pot;
        return;
    }

    node->type = NodeType::PLAYER;
    node->player = acting_player;
    node->node_id = next_node_id_++;
    node->street = street;
    node->pot = pot;
    node->current_bet = current_bet;
    node->num_players = num_players;
    std::copy(stacks, stacks + 6, node->stacks);
    std::copy(bets, bets + 6, node->bets);
    std::copy(folded, folded + 6, node->folded);
    std::copy(all_in, all_in + 6, node->all_in);

    auto actions = get_valid_actions(
        acting_player, stacks[acting_player],
        current_bet, bets[acting_player],
        pot, num_raises, config_.max_raises_per_street
    );

    node->valid_actions = actions;

    for (auto action : actions) {
        auto child = std::make_unique<GameNode>();

        // Copy state
        int new_stacks[6], new_bets[6];
        bool new_folded[6], new_all_in[6];
        std::copy(stacks, stacks + 6, new_stacks);
        std::copy(bets, bets + 6, new_bets);
        std::copy(folded, folded + 6, new_folded);
        std::copy(all_in, all_in + 6, new_all_in);
        int new_pot = pot;
        int new_current_bet = current_bet;
        int new_last_raiser = last_raiser;
        int new_num_raises = num_raises;

        switch (action) {
            case ActionType::FOLD:
                new_folded[acting_player] = true;
                break;

            case ActionType::CHECK:
                break;

            case ActionType::CALL: {
                int to_call = current_bet - bets[acting_player];
                int actual = std::min(to_call, new_stacks[acting_player]);
                new_stacks[acting_player] -= actual;
                new_bets[acting_player] += actual;
                new_pot += actual;
                if (new_stacks[acting_player] == 0) new_all_in[acting_player] = true;
                break;
            }

            case ActionType::RAISE_HALF:
            case ActionType::RAISE_POT:
            case ActionType::RAISE_2X: {
                double mult = BET_SIZES[static_cast<int>(action)];
                int raise_amount = static_cast<int>(pot * mult);
                int raise_to = current_bet + std::max(raise_amount, config_.big_blind);
                int cost = raise_to - bets[acting_player];
                cost = std::min(cost, new_stacks[acting_player]);
                new_stacks[acting_player] -= cost;
                new_bets[acting_player] += cost;
                new_pot += cost;
                new_current_bet = new_bets[acting_player];
                new_last_raiser = acting_player;
                new_num_raises++;
                if (new_stacks[acting_player] == 0) new_all_in[acting_player] = true;
                break;
            }

            case ActionType::ALL_IN: {
                int cost = new_stacks[acting_player];
                new_bets[acting_player] += cost;
                new_pot += cost;
                new_stacks[acting_player] = 0;
                if (new_bets[acting_player] > new_current_bet) {
                    new_current_bet = new_bets[acting_player];
                    new_last_raiser = acting_player;
                    new_num_raises++;
                }
                new_all_in[acting_player] = true;
                break;
            }

            default:
                break;
        }

        // Determine next player or next street.
        // close_player: the player who gets the last action before the street ends.
        // Preflop: BB (has option). Post-flop: first-to-act after a full round.
        // After a raise: raiser becomes close_player.
        
        int new_close = close_player;
        if (action == ActionType::RAISE_HALF || action == ActionType::RAISE_POT ||
            action == ActionType::RAISE_2X || 
            (action == ActionType::ALL_IN && new_bets[acting_player] > current_bet)) {
            // Raise reopens action; raiser closes
            new_close = acting_player;
        }
        
        int next = next_active_player(acting_player, new_folded, new_all_in, num_players);
        
        bool street_done = false;
        if (next == -1) {
            // No active players
            street_done = true;
        } else if (acting_player == new_close && 
                   (action == ActionType::CHECK || action == ActionType::CALL || action == ActionType::FOLD)) {
            // The close_player acted without raising -> street done
            street_done = true;
        }
        if (street_done || count_in_hand(new_folded, num_players) <= 1) {
            if (count_in_hand(new_folded, num_players) <= 1) {
                // Terminal
                child->type = NodeType::TERMINAL;
                child->street = street;
                child->pot = new_pot;
            } else if (street == Street::RIVER) {
                // Showdown
                child->type = NodeType::TERMINAL;
                child->street = street;
                child->pot = new_pot;
            } else {
                // Next street
                Street next_street = static_cast<Street>(static_cast<int>(street) + 1);
                // Reset bets for new street
                int reset_bets[6] = {};
                int first_post = next_active_player(button, new_folded, new_all_in, num_players);
                if (first_post >= 0) {
                    // On a new street, close_player = the last active player
                    // before first_post (i.e. first_post closes after full round)
                    // Actually: find the player just BEFORE first_post
                    int last_before_first = first_post;
                    for (int i = 1; i < num_players; ++i) {
                        int idx = (first_post - i + num_players) % num_players;
                        if (!new_folded[idx] && !new_all_in[idx]) {
                            last_before_first = idx;
                            break;
                        }
                    }
                    build_recursive(
                        child.get(), next_street, first_post,
                        new_pot, new_stacks, reset_bets,
                        new_folded, new_all_in,
                        0, -1, 0, num_players, button,
                        last_before_first
                    );
                } else {
                    child->type = NodeType::TERMINAL;
                    child->street = next_street;
                    child->pot = new_pot;
                }
            }
        } else {
            build_recursive(
                child.get(), street, next,
                new_pot, new_stacks, new_bets,
                new_folded, new_all_in,
                new_current_bet, new_last_raiser, new_num_raises,
                num_players, button, new_close
            );
        }

        node->children.push_back(std::move(child));
    }
}

} // namespace poker
