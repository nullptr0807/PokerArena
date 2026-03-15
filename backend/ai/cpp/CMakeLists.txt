cmake_minimum_required(VERSION 3.16)
project(poker_ai_engine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Core library
add_library(poker_ai STATIC
    src/mccfr.cpp
    src/hand_abstraction.cpp
    src/subgame_solver.cpp
)
target_include_directories(poker_ai PUBLIC include)

# Standalone trainer binary
add_executable(train_blueprint src/train_main.cpp)
target_link_libraries(train_blueprint poker_ai)

# pybind11 module (optional, requires pybind11)
find_package(pybind11 QUIET)
if(pybind11_FOUND)
    pybind11_add_module(poker_ai_py src/bindings.cpp)
    target_link_libraries(poker_ai_py PRIVATE poker_ai)
    message(STATUS "pybind11 found — building Python bindings")
else()
    message(STATUS "pybind11 not found — skipping Python bindings")
endif()
