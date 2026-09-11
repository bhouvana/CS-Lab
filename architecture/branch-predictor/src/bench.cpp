// Experiment: which predictor handles which branch pattern best?
#include <iomanip>
#include <iostream>

#include "patterns.hpp"
#include "predictors.hpp"

namespace {
void run_pattern(const std::string& label, const std::vector<Outcome>& trace) {
    std::cout << label << " (" << trace.size() << " branches)\n";
    std::cout << "Predictor            Accuracy\n";
    std::cout << "--------------------------------\n";
    for (auto& predictor : make_all_predictors()) {
        double acc = evaluate_predictor(*predictor, trace);
        std::cout << std::left << std::setw(21) << predictor->name() << std::right << std::fixed
                   << std::setprecision(1) << std::setw(5) << (100.0 * acc) << "%\n";
    }
    std::cout << "\n";
}
} // namespace

int main() {
    const int n = 2000;
    std::cout << "Benchmark: predictor accuracy across synthetic branch patterns\n\n";

    run_pattern("Always taken", pattern_always_taken(n));
    run_pattern("Alternating (T N T N ...)", pattern_alternating(n));
    run_pattern("Loop (9 taken, 1 not-taken, repeating)", pattern_loop(n, 10));
    run_pattern("Random (50% taken)", pattern_random(n, 0.5, 42));
    run_pattern("Biased (90% taken)", pattern_random(n, 0.9, 42));

    return 0;
}
