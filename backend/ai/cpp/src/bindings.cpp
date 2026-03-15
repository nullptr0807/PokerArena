#include "mccfr.h"
#include "hand_abstraction.h"
#include "subgame_solver.h"
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(poker_ai_py, m) {
    m.doc() = "PokerArena C++ AI Engine";

    py::class_<poker::MCCFRTrainer>(m, "MCCFRTrainer")
        .def(py::init<int>(), py::arg("num_players") = 2)
        .def("train", &poker::MCCFRTrainer::train, py::arg("iterations"))
        .def("train_parallel", &poker::MCCFRTrainer::train_parallel, py::arg("iterations"), py::arg("num_threads"))
        .def("query", &poker::MCCFRTrainer::query, py::arg("info_set_key"))
        .def("save", &poker::MCCFRTrainer::save, py::arg("path"))
        .def("load", &poker::MCCFRTrainer::load, py::arg("path"))
        .def("num_info_sets", &poker::MCCFRTrainer::num_info_sets);

    py::class_<poker::SubgameState>(m, "SubgameState")
        .def(py::init<>())
        .def_readwrite("hole_cards", &poker::SubgameState::hole_cards)
        .def_readwrite("board", &poker::SubgameState::board)
        .def_readwrite("pot", &poker::SubgameState::pot)
        .def_readwrite("current_bet", &poker::SubgameState::current_bet)
        .def_readwrite("my_chips", &poker::SubgameState::my_chips)
        .def_readwrite("opp_chips", &poker::SubgameState::opp_chips)
        .def_readwrite("action_history", &poker::SubgameState::action_history);

    py::class_<poker::SubgameSolver>(m, "SubgameSolver")
        .def(py::init<const poker::MCCFRTrainer&, int>(),
             py::arg("blueprint"), py::arg("depth_limit") = 4)
        .def("solve", &poker::SubgameSolver::solve,
             py::arg("state"), py::arg("iterations") = 10000);

    m.def("compute_ehs", [](std::array<int, 2> hole, std::vector<int> board, int rollouts) {
        return poker::compute_ehs(hole, board, rollouts);
    }, py::arg("hole"), py::arg("board"), py::arg("rollouts") = 1000);

    m.def("hand_to_bucket", [](std::array<int, 2> hole, std::vector<int> board) {
        return poker::hand_to_bucket(hole, board);
    }, py::arg("hole"), py::arg("board"));
}
