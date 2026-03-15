/**
 * Decision server: reads JSON requests from stdin, writes JSON responses to stdout.
 * This is the subprocess interface between Python backend and C++ engine.
 *
 * Protocol:
 * Request (one JSON per line):
 * {
 *   "action": "decide",
 *   "difficulty": "normal" | "medium" | "advanced",
 *   "hole_cards": ["As", "Kh"],
 *   "board": ["Qs", "Js", "Ts"],
 *   "pot": 100,
 *   "stacks": [200, 150, 180, 200, 190, 195],
 *   "current_bet": 20,
 *   "player_bet": 10,
 *   "player": 0,
 *   "num_players": 6,
 *   "valid_actions": ["fold", "call", "raise_33", "raise_75", "all_in"]
 * }
 *
 * Response:
 * {
 *   "action": "call",
 *   "amount": 10,
 *   "confidence": 0.45,
 *   "reasoning": "call with confidence 0.45"
 * }
 *
 * Special requests:
 * {"action": "load", "path": "data/blueprint.bin"}
 * {"action": "quit"}
 */

#include "strategy.h"
#include "card.h"

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

// Minimal JSON parsing (no external dependency)
// In production, use nlohmann/json or similar

static std::string trim(const std::string& s) {
    auto start = s.find_first_not_of(" \t\n\r\"");
    auto end = s.find_last_not_of(" \t\n\r\"");
    if (start == std::string::npos) return "";
    return s.substr(start, end - start + 1);
}

static std::string get_json_string(const std::string& json, const std::string& key) {
    auto pos = json.find("\"" + key + "\"");
    if (pos == std::string::npos) return "";
    pos = json.find(":", pos);
    if (pos == std::string::npos) return "";
    pos = json.find("\"", pos);
    if (pos == std::string::npos) return "";
    auto end = json.find("\"", pos + 1);
    return json.substr(pos + 1, end - pos - 1);
}

static int get_json_int(const std::string& json, const std::string& key) {
    auto pos = json.find("\"" + key + "\"");
    if (pos == std::string::npos) return 0;
    pos = json.find(":", pos);
    if (pos == std::string::npos) return 0;
    pos++;
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) pos++;
    return std::stoi(json.substr(pos));
}

static std::vector<std::string> get_json_string_array(const std::string& json, const std::string& key) {
    std::vector<std::string> result;
    auto pos = json.find("\"" + key + "\"");
    if (pos == std::string::npos) return result;
    pos = json.find("[", pos);
    if (pos == std::string::npos) return result;
    auto end = json.find("]", pos);
    if (end == std::string::npos) return result;
    std::string arr = json.substr(pos + 1, end - pos - 1);

    std::stringstream ss(arr);
    std::string item;
    while (std::getline(ss, item, ',')) {
        std::string trimmed = trim(item);
        if (!trimmed.empty()) result.push_back(trimmed);
    }
    return result;
}

static std::vector<int> get_json_int_array(const std::string& json, const std::string& key) {
    std::vector<int> result;
    auto pos = json.find("\"" + key + "\"");
    if (pos == std::string::npos) return result;
    pos = json.find("[", pos);
    if (pos == std::string::npos) return result;
    auto end = json.find("]", pos);
    if (end == std::string::npos) return result;
    std::string arr = json.substr(pos + 1, end - pos - 1);

    std::stringstream ss(arr);
    std::string item;
    while (std::getline(ss, item, ',')) {
        std::string trimmed = trim(item);
        if (!trimmed.empty()) result.push_back(std::stoi(trimmed));
    }
    return result;
}

static poker::ActionType parse_action_type(const std::string& s) {
    if (s == "fold") return poker::ActionType::FOLD;
    if (s == "check") return poker::ActionType::CHECK;
    if (s == "call") return poker::ActionType::CALL;
    if (s == "raise_33") return poker::ActionType::RAISE_HALF;
    if (s == "raise_75") return poker::ActionType::RAISE_POT;
    if (s == "raise_150") return poker::ActionType::RAISE_2X;
    if (s == "all_in") return poker::ActionType::ALL_IN;
    return poker::ActionType::FOLD;
}

static poker::Difficulty parse_difficulty(const std::string& s) {
    if (s == "normal") return poker::Difficulty::NORMAL;
    if (s == "medium") return poker::Difficulty::MEDIUM;
    if (s == "advanced") return poker::Difficulty::ADVANCED;
    return poker::Difficulty::NORMAL;
}

int main() {
    poker::StrategyManager::Config config;
    poker::StrategyManager manager(config);

    std::cerr << "Poker Engine Decision Server ready." << std::endl;
    std::cout << R"({"status":"ready"})" << std::endl;

    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) continue;

        std::string action = get_json_string(line, "action");

        if (action == "quit") {
            std::cout << R"({"status":"bye"})" << std::endl;
            break;
        }

        if (action == "load") {
            std::string path = get_json_string(line, "path");
            bool ok = manager.load_blueprint(path);
            std::cout << R"({"status":")" << (ok ? "loaded" : "error") << R"("})" << std::endl;
            continue;
        }

        if (action == "decide") {
            auto difficulty = parse_difficulty(get_json_string(line, "difficulty"));
            auto hole_strs = get_json_string_array(line, "hole_cards");
            auto board_strs = get_json_string_array(line, "board");
            int pot = get_json_int(line, "pot");
            auto stacks_vec = get_json_int_array(line, "stacks");
            int current_bet = get_json_int(line, "current_bet");
            int player_bet = get_json_int(line, "player_bet");
            int player = get_json_int(line, "player");
            int num_players = get_json_int(line, "num_players");
            auto valid_strs = get_json_string_array(line, "valid_actions");

            poker::Card hole[2] = {
                poker::string_to_card(hole_strs[0]),
                poker::string_to_card(hole_strs[1])
            };

            poker::Card board[5] = {};
            for (int i = 0; i < static_cast<int>(board_strs.size()); ++i) {
                board[i] = poker::string_to_card(board_strs[i]);
            }

            int stacks[6] = {};
            for (int i = 0; i < static_cast<int>(stacks_vec.size()) && i < 6; ++i) {
                stacks[i] = stacks_vec[i];
            }

            std::vector<poker::ActionType> valid_actions;
            for (const auto& s : valid_strs) {
                valid_actions.push_back(parse_action_type(s));
            }

            auto decision = manager.decide(
                difficulty, hole, board, static_cast<int>(board_strs.size()),
                pot, stacks, current_bet, player_bet, player, num_players,
                valid_actions
            );

            std::cout << R"({"action":")" << poker::action_name(decision.action)
                      << R"(","amount":)" << decision.amount
                      << R"(,"confidence":)" << decision.confidence
                      << R"(,"reasoning":")" << decision.reasoning
                      << R"("})" << std::endl;
            continue;
        }

        std::cout << R"({"error":"unknown action"})" << std::endl;
    }

    return 0;
}
