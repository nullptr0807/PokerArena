cmake_minimum_required(VERSION 3.20)
project(PokerEngine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Optimization flags
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -DNDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -fsanitize=address,undefined")

# ─── Core library ─────────────────────────────────────────────
add_library(poker_engine STATIC
    src/card.cpp
    src/hand_eval.cpp
    src/abstraction.cpp
    src/game_tree.cpp
    src/cfr.cpp
    src/subgame.cpp
    src/strategy.cpp
)
target_include_directories(poker_engine PUBLIC include)

# ─── Training binary ─────────────────────────────────────────
add_executable(train src/train_main.cpp)
target_link_libraries(train poker_engine)

# ─── Decision server (subprocess JSON interface) ─────────────
add_executable(decide src/decide_main.cpp)
target_link_libraries(decide poker_engine)

# ─── Tests ────────────────────────────────────────────────────
enable_testing()
add_executable(engine_tests tests/test_main.cpp)
target_link_libraries(engine_tests poker_engine)
add_test(NAME engine_tests COMMAND engine_tests)
