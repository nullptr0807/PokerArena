#include "cfr.h"
#include "abstraction.h"

#include <iostream>
#include <string>
#include <filesystem>

int main(int argc, char* argv[]) {
    std::cout << "=== Poker Arena Blueprint Trainer ===" << std::endl;

    poker::MCCFRTrainer::Config config;
    config.num_players = 6;
    config.iterations = 1000000;
    config.checkpoint_every = 100000;
    config.checkpoint_dir = "data";

    std::string resume_path;

    // Parse args
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--players" && i + 1 < argc) {
            config.num_players = std::stoi(argv[++i]);
        } else if (arg == "--iterations" && i + 1 < argc) {
            config.iterations = std::stoi(argv[++i]);
        } else if (arg == "--output" && i + 1 < argc) {
            config.checkpoint_dir = argv[++i];
        } else if (arg == "--checkpoint-every" && i + 1 < argc) {
            config.checkpoint_every = std::stoi(argv[++i]);
        } else if (arg == "--resume" && i + 1 < argc) {
            resume_path = argv[++i];
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: train [options]\n"
                      << "  --players N          Number of players (default: 6)\n"
                      << "  --iterations N       Training iterations (default: 1000000)\n"
                      << "  --output DIR         Output directory (default: data)\n"
                      << "  --checkpoint-every N Save every N iterations (default: 100000)\n"
                      << "  --resume PATH        Resume from checkpoint file\n";
            return 0;
        }
    }

    // Create output dir
    std::filesystem::create_directories(config.checkpoint_dir);

    std::cout << "Players: " << config.num_players << std::endl;
    std::cout << "Iterations: " << config.iterations << std::endl;
    std::cout << "Checkpoint every: " << config.checkpoint_every << std::endl;
    std::cout << "Output: " << config.checkpoint_dir << std::endl;

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
    poker::MCCFRTrainer trainer(config);

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
