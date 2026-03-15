#pragma once
/**
 * Real-time subgame solving for Hard difficulty.
 *
 * Given the current game state and a blueprint strategy,
 * re-solves the remaining subgame with the actual board cards
 * for a more refined strategy than the abstracted blueprint.
 */

#include "mccfr.h"
#include <array>
#include <vector>

namespace poker {

struct SubgameState {
    std::array<int, 2> hole_cards;
    std::vector<int> board;
    int pot;
    int current_bet;
    int my_chips;
    int opp_chips;
    std::string action_history;
};

/**
 * Subgame solver using depth-limited MCCFR.
 */
class SubgameSolver {
public:
    SubgameSolver(const MCCFRTrainer& blueprint, int depth_limit = 4);

    /**
     * Solve the subgame and return action probabilities.
     *
     * @param state Current game state
     * @param iterations Number of MCCFR iterations for the subgame
     * @return Action probabilities (same indices as NUM_ACTIONS)
     */
    std::array<double, NUM_ACTIONS> solve(
        const SubgameState& state,
        int iterations = 10000
    );

private:
    const MCCFRTrainer& blueprint_;
    int depth_limit_;
};

} // namespace poker
