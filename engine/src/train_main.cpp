#include "cfr.h"
#include "abstraction.h"

#include <chrono>
#include <ctime>
#include <iostream>
#include <sstream>
#include <string>
#include <filesystem>

// Generate output dir name from params: run_YYYYMMDD_HHMMSS_p2_fb500_tb500_rb800_ehs300_bs5_i3000000
std::string make_run_dir(const poker::MCCFRTrainer::Config& config) {
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y%m%d_%H%M%S", std::localtime(&t));

    std::ostringstream oss;
    oss << "runs/run_" << buf
        << "_p" << config.num_players
        << "_fb" << config.abstraction_config.flop_buckets
        << "_tb" << config.abstraction_config.turn_buckets
        << "_rb" << config.abstraction_config.river_buckets
        << "_ehs" << config.abstraction_config.ehs_samples
        << "_bs" << config.tree_config.bet_sizes.size()
        << "_i" << config.iterations;
    return oss.str();
}

int main(int argc, char* argv[]) {
    std::cout << "=== Poker Arena Blueprint Trainer ===" << std::endl;

    poker::MCCFRTrainer::Config config;
    config.num_players = 2;
    config.iterations = 3000000;
    config.checkpoint_every = 100000;
    config.checkpoint_dir = "";  // auto-generate if not specified

    // Abstraction defaults (NL50 target)
    config.abstraction_config.flop_buckets = 500;
    config.abstraction_config.turn_buckets = 500;
    config.abstraction_config.river_buckets = 800;
    config.abstraction_config.ehs_samples = 300;

    std::string resume_path;
    bool auto_output = true;

    // Parse args
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--players" && i + 1 < argc) {
            config.num_players = std::stoi(argv[++i]);
        } else if (arg == "--iterations" && i + 1 < argc) {
            config.iterations = std::stoi(argv[++i]);
        } else if (arg == "--output" && i + 1 < argc) {
            config.checkpoint_dir = argv[++i];
            auto_output = false;
        } else if (arg == "--checkpoint-every" && i + 1 < argc) {
            config.checkpoint_every = std::stoi(argv[++i]);
        } else if (arg == "--resume" && i + 1 < argc) {
            resume_path = argv[++i];
        } else if (arg == "--flop-buckets" && i + 1 < argc) {
            config.abstraction_config.flop_buckets = std::stoi(argv[++i]);
        } else if (arg == "--turn-buckets" && i + 1 < argc) {
            config.abstraction_config.turn_buckets = std::stoi(argv[++i]);
        } else if (arg == "--river-buckets" && i + 1 < argc) {
            config.abstraction_config.river_buckets = std::stoi(argv[++i]);
        } else if (arg == "--ehs-samples" && i + 1 < argc) {
            config.abstraction_config.ehs_samples = std::stoi(argv[++i]);
        } else if (arg == "--log-every" && i + 1 < argc) {
            config.log_every = std::stoi(argv[++i]);
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: train [options]\n"
                      << "\nGeneral:\n"
                      << "  --players N          Number of players (default: 2)\n"
                      << "  --iterations N       Training iterations (default: 3000000)\n"
                      << "  --output DIR         Output directory (default: auto-generated)\n"
                      << "  --checkpoint-every N Save every N iterations (default: 100000)\n"
                      << "  --resume PATH        Resume from checkpoint file\n"
                      << "\nAbstraction:\n"
                      << "  --flop-buckets N     Flop buckets (default: 500)\n"
                      << "  --turn-buckets N     Turn buckets (default: 500)\n"
                      << "  --river-buckets N    River buckets (default: 800)\n"
                      << "  --ehs-samples N      EHS² MC samples (default: 300)\n"
                      << "\nLogging:\n"
                      << "  --log-every N        Log progress every N iterations (default: 10000)\n"
                      << "\nOutput dir is auto-generated with parameters in the name\n"
                      << "unless --output is specified. All runs saved under runs/\n";
            return 0;
        }
    }

    // Auto-generate output dir if not specified
    if (auto_output) {
        config.checkpoint_dir = make_run_dir(config);
    }

    // Create output dir
    std::filesystem::create_directories(config.checkpoint_dir);

    // Set log file path
    config.log_file = config.checkpoint_dir + "/training.log";

    // Print config summary
    std::cout << "\n── Config ──────────────────────────────" << std::endl;
    std::cout << "Players:        " << config.num_players << std::endl;
    std::cout << "Iterations:     " << config.iterations << std::endl;
    std::cout << "Checkpoint:     every " << config.checkpoint_every << std::endl;
    std::cout << "Flop buckets:   " << config.abstraction_config.flop_buckets << std::endl;
    std::cout << "Turn buckets:   " << config.abstraction_config.turn_buckets << std::endl;
    std::cout << "River buckets:  " << config.abstraction_config.river_buckets << std::endl;
    std::cout << "EHS² samples:   " << config.abstraction_config.ehs_samples << std::endl;
    std::cout << "Bet sizes:      [";
    for (size_t i = 0; i < config.tree_config.bet_sizes.size(); ++i) {
        if (i > 0) std::cout << ", ";
        std::cout << config.tree_config.bet_sizes[i] << "x";
    }
    std::cout << "]" << std::endl;
    std::cout << "Variant:        CFR+" << std::endl;
    std::cout << "Output:         " << config.checkpoint_dir << std::endl;
    std::cout << "────────────────────────────────────────" << std::endl;

    // Train or load abstraction
    std::string abs_path = config.checkpoint_dir + "/abstraction.bin";
    poker::HandAbstraction abstraction(config.abstraction_config);

    if (std::filesystem::exists(abs_path)) {
        std::cout << "\nLoading existing abstraction from " << abs_path << std::endl;
        abstraction.load(abs_path);
    } else {
        std::cout << "\n--- Training hand abstraction ---" << std::endl;
        abstraction.train(20);
        abstraction.save(abs_path);
    }

    // Create trainer
    config.tree_config.num_players = config.num_players;
    poker::MCCFRTrainer trainer(config);

    // Load pre-trained abstraction into trainer
    trainer.load_abstraction(abs_path);
    std::cout << "Loaded abstraction into trainer (EHS² bucketing active)." << std::endl;

    // Resume from checkpoint if specified
    if (!resume_path.empty()) {
        std::cout << "\nResuming from " << resume_path << std::endl;
        if (!trainer.load(resume_path)) {
            std::cerr << "ERROR: Failed to load checkpoint: " << resume_path << std::endl;
            return 1;
        }
        std::cout << "Loaded. Previous iterations: " << trainer.total_iterations()
                  << " | Info sets: " << trainer.num_info_sets() << std::endl;
    } else {
        // Auto-detect latest checkpoint
        int64_t latest = 0;
        std::string latest_path;
        for (auto& entry : std::filesystem::directory_iterator(config.checkpoint_dir)) {
            auto name = entry.path().filename().string();
            if (name.starts_with("checkpoint_") && name.ends_with(".bin")) {
                auto num_str = name.substr(11, name.size() - 15);
                try {
                    int64_t n = std::stoll(num_str);
                    if (n > latest) { latest = n; latest_path = entry.path().string(); }
                } catch (...) {}
            }
        }
        if (latest > 0) {
            std::cout << "\nFound latest checkpoint: " << latest_path
                      << " (" << latest << " iterations)" << std::endl;
            std::cout << "Auto-resuming..." << std::endl;
            if (trainer.load(latest_path)) {
                std::cout << "Loaded. Info sets: " << trainer.num_info_sets() << std::endl;
            } else {
                std::cerr << "Warning: Failed to load, starting fresh." << std::endl;
            }
        }
    }

    // Train
    int remaining = config.iterations - static_cast<int>(trainer.total_iterations());
    if (remaining <= 0) {
        std::cout << "\nAlready completed " << trainer.total_iterations()
                  << " iterations (target: " << config.iterations << "). Nothing to do." << std::endl;
    } else {
        std::cout << "\n--- Training " << remaining << " more iterations ---" << std::endl;
        trainer.train(remaining);
    }

    // Save final
    std::string final_path = config.checkpoint_dir + "/blueprint.bin";
    trainer.save(final_path);
    std::cout << "\nTraining complete. Blueprint saved to " << final_path << std::endl;
    std::cout << "Total iterations: " << trainer.total_iterations()
              << " | Info sets: " << trainer.num_info_sets() << std::endl;

    return 0;
}
