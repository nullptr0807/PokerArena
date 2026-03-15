/**
 * Standalone training binary for blueprint strategy.
 * Usage: ./train_blueprint <iterations> <output_path> [--resume <path>] [--checkpoint-interval <N>]
 */

#include "mccfr.h"
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <thread>

static std::string iso_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&t), "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

static void save_meta(const std::string& base_path, int64_t total_iters, size_t info_sets) {
    std::string meta_path = base_path;
    // Replace .bin with .meta if it ends with .bin, else append .meta
    if (meta_path.size() > 4 && meta_path.substr(meta_path.size() - 4) == ".bin") {
        meta_path = meta_path.substr(0, meta_path.size() - 4) + ".meta";
    } else {
        meta_path += ".meta";
    }
    std::ofstream out(meta_path);
    out << "{\"total_iterations\": " << total_iters
        << ", \"info_sets\": " << info_sets
        << ", \"timestamp\": \"" << iso_timestamp() << "\"}\n";
}

static int64_t load_meta_iterations(const std::string& base_path) {
    std::string meta_path = base_path;
    if (meta_path.size() > 4 && meta_path.substr(meta_path.size() - 4) == ".bin") {
        meta_path = meta_path.substr(0, meta_path.size() - 4) + ".meta";
    } else {
        meta_path += ".meta";
    }
    std::ifstream in(meta_path);
    if (!in) return 0;
    // Simple parse: find "total_iterations": <number>
    std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    auto pos = content.find("\"total_iterations\":");
    if (pos == std::string::npos) return 0;
    pos += strlen("\"total_iterations\":");
    while (pos < content.size() && content[pos] == ' ') pos++;
    return std::atoll(content.c_str() + pos);
}

int main(int argc, char* argv[]) {
    int64_t target_iterations = 1000000;
    std::string output = "blueprint.bin";
    std::string resume_path;
    int64_t checkpoint_interval = 100000;
    int num_threads = std::max(1, static_cast<int>(std::thread::hardware_concurrency()) - 2);

    // Parse args
    if (argc > 1) target_iterations = std::atoll(argv[1]);
    if (argc > 2) output = argv[2];
    for (int i = 3; i < argc; ++i) {
        if (std::strcmp(argv[i], "--resume") == 0 && i + 1 < argc) {
            resume_path = argv[++i];
        } else if (std::strcmp(argv[i], "--checkpoint-interval") == 0 && i + 1 < argc) {
            checkpoint_interval = std::atoll(argv[++i]);
        } else if (std::strcmp(argv[i], "--threads") == 0 && i + 1 < argc) {
            num_threads = std::max(1, std::atoi(argv[++i]));
        }
    }

    std::cout << "MCCFR Blueprint Trainer\n";
    std::cout << "  Target iterations:     " << target_iterations << "\n";
    std::cout << "  Output:                " << output << "\n";
    std::cout << "  Checkpoint interval:   " << checkpoint_interval << "\n";
    std::cout << "  Threads:               " << num_threads << "\n";

    poker::MCCFRTrainer trainer(2);
    int64_t completed = 0;

    if (!resume_path.empty()) {
        std::cout << "  Resuming from:         " << resume_path << "\n";
        trainer.load(resume_path);
        completed = load_meta_iterations(resume_path);
        std::cout << "  Previously completed:  " << completed << " iterations\n";
        std::cout << "  Info sets loaded:      " << trainer.num_info_sets() << "\n";
    }

    int64_t remaining = target_iterations - completed;
    if (remaining <= 0) {
        std::cout << "\nAlready completed " << completed << " >= " << target_iterations << " target. Nothing to do.\n";
        return 0;
    }

    std::cout << "\nTraining " << remaining << " more iterations...\n\n";

    auto t0 = std::chrono::steady_clock::now();
    int64_t since_checkpoint = 0;
    int64_t pct_interval = std::max(remaining / 100, (int64_t)1);
    // Train in chunks for progress reporting
    int64_t chunk_size = std::max(pct_interval, (int64_t)10);
    int64_t done = 0;

    while (done < remaining) {
        int64_t batch = std::min(chunk_size, remaining - done);
        trainer.train_parallel(batch, num_threads);
        done += batch;
        completed += batch;
        since_checkpoint += batch;

        // Progress bar
        int pct = static_cast<int>(done * 100 / remaining);
        pct = std::min(pct, 100);
        auto now = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration<double>(now - t0).count();
        double rate = done / std::max(elapsed, 0.001);
        double eta = (remaining - done) / std::max(rate, 1.0);

        int bar_width = 30;
        int filled = pct * bar_width / 100;
        std::cout << "\r  [";
        for (int b = 0; b < bar_width; ++b)
            std::cout << (b < filled ? "█" : "░");
        std::cout << "] " << std::setw(3) << pct << "% | "
                  << completed << "/" << target_iterations
                  << " | " << std::fixed << std::setprecision(0) << rate << " iter/s"
                  << " | ETA " << std::setprecision(0) << eta << "s"
                  << std::flush;

        if (since_checkpoint >= checkpoint_interval) {
            since_checkpoint = 0;
            trainer.save(output);
            save_meta(output, completed, trainer.num_info_sets());
            std::cout << "\n  [checkpoint saved at " << completed
                      << " | info_sets " << trainer.num_info_sets() << "]\n";
        }
    }
    std::cout << "\n";

    // Final save
    trainer.save(output);
    save_meta(output, completed, trainer.num_info_sets());

    auto t1 = std::chrono::steady_clock::now();
    double total_time = std::chrono::duration<double>(t1 - t0).count();

    std::cout << "\nDone. Total iterations: " << completed
              << " | Info sets: " << trainer.num_info_sets()
              << " | Time: " << std::fixed << std::setprecision(1) << total_time << "s\n";
    std::cout << "Saved to " << output << "\n";

    return 0;
}
