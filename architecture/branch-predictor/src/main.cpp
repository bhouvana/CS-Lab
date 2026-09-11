// CLI: branch [--predictor always-taken|always-not-taken|1-bit|2-bit|gshare] <trace-file>
// With no --predictor, runs every predictor and prints a comparison table.
#include <cstring>
#include <iomanip>
#include <iostream>

#include "patterns.hpp"
#include "predictors.hpp"

namespace {
std::unique_ptr<Predictor> make_predictor(const std::string& name) {
    if (name == "always-taken") return std::make_unique<AlwaysTaken>();
    if (name == "always-not-taken") return std::make_unique<AlwaysNotTaken>();
    if (name == "1-bit") return std::make_unique<OneBitPredictor>();
    if (name == "2-bit") return std::make_unique<TwoBitPredictor>();
    if (name == "gshare") return std::make_unique<GSharePredictor>();
    return nullptr;
}
} // namespace

int main(int argc, char** argv) {
    std::string predictor_name;
    std::string trace_path;

    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "--predictor") == 0 && i + 1 < argc) predictor_name = argv[++i];
        else trace_path = argv[i];
    }

    if (trace_path.empty()) {
        std::cerr << "usage: " << argv[0]
                  << " [--predictor always-taken|always-not-taken|1-bit|2-bit|gshare] <trace-file>\n";
        return 2;
    }

    std::vector<Outcome> trace;
    try {
        trace = load_trace_file(trace_path);
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }

    if (!predictor_name.empty()) {
        auto predictor = make_predictor(predictor_name);
        if (!predictor) {
            std::cerr << "unknown predictor '" << predictor_name << "'\n";
            return 2;
        }
        double accuracy = evaluate_predictor(*predictor, trace);
        std::cout << predictor->name() << ": " << std::fixed << std::setprecision(1) << (100.0 * accuracy)
                  << "% (" << trace.size() << " branches)\n";
        return 0;
    }

    std::cout << "Predictor            Accuracy\n";
    std::cout << "--------------------------------\n";
    for (auto& predictor : make_all_predictors()) {
        double accuracy = evaluate_predictor(*predictor, trace);
        std::cout << std::left << std::setw(21) << predictor->name() << std::right << std::fixed
                   << std::setprecision(1) << std::setw(5) << (100.0 * accuracy) << "%\n";
    }
    return 0;
}
