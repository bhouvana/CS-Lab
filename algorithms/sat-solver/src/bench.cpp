// Experiment: how does clause count affect solver difficulty?
//
// Random 3-SAT famously has a "phase transition" around a
// clauses-to-variables ratio of ~4.27: well below it almost every
// formula is satisfiable (and easy — usually a short chain of unit
// propagations settles it), well above it almost every formula is
// unsatisfiable (and also easy, because contradictions surface fast),
// but right at the threshold formulas are maximally hard to decide
// either way, and DPLL's backtrack count should visibly spike there.
#include <iomanip>
#include <iostream>
#include <random>

#include "sat.hpp"

namespace {
std::vector<std::vector<int>> random_3sat(int num_vars, int num_clauses, std::mt19937& rng) {
    std::uniform_int_distribution<int> var_dist(1, num_vars);
    std::uniform_int_distribution<int> sign_dist(0, 1);
    std::vector<std::vector<int>> clauses;
    clauses.reserve(num_clauses);
    for (int c = 0; c < num_clauses; c++) {
        std::vector<int> clause;
        while ((int)clause.size() < 3) {
            int v = var_dist(rng);
            bool already = false;
            for (int lit : clause)
                if (std::abs(lit) == v) already = true;
            if (already) continue; // keep literals within a clause on distinct variables
            clause.push_back(sign_dist(rng) ? v : -v);
        }
        clauses.push_back(clause);
    }
    return clauses;
}
} // namespace

int main() {
    const int num_vars = 20;
    const int samples_per_ratio = 20;
    std::mt19937 rng(42); // fixed seed: reproducible experiment

    std::cout << "Benchmark: random 3-SAT difficulty vs. clauses/variables ratio\n";
    std::cout << "(" << num_vars << " variables, " << samples_per_ratio << " random instances per ratio)\n\n";
    std::cout << std::left << std::setw(10) << "ratio" << std::setw(10) << "clauses" << std::setw(10) << "%SAT"
               << std::setw(14) << "avg decisions" << std::setw(16) << "avg backtracks" << "avg ms" << "\n";
    // Set once, up front: std::fixed/setprecision are sticky stream state
    // in C++ (they leak into later statements, not just later values in
    // the same one), so setting them mid-loop made only the *first* row
    // print unformatted and every row after it inconsistently formatted.
    std::cout << std::fixed << std::setprecision(2);

    double ratios[] = {2.0, 3.0, 3.5, 4.0, 4.27, 4.5, 5.0, 6.0, 8.0};
    for (double ratio : ratios) {
        int num_clauses = (int)(ratio * num_vars);
        long long total_decisions = 0, total_backtracks = 0, sat_count = 0;
        double total_ms = 0.0;

        for (int s = 0; s < samples_per_ratio; s++) {
            auto clauses = random_3sat(num_vars, num_clauses, rng);
            Solver solver(num_vars, clauses);
            bool sat = solver.solve();
            const SolveStats& st = solver.stats();
            total_decisions += st.decisions;
            total_backtracks += st.backtracks;
            total_ms += st.runtime_ms;
            if (sat) sat_count++;
        }

        std::cout << std::left << std::setw(10) << ratio << std::setw(10) << num_clauses
                   << std::setw(10) << (100.0 * sat_count / samples_per_ratio)
                   << std::setw(14) << (double)total_decisions / samples_per_ratio
                   << std::setw(16) << (double)total_backtracks / samples_per_ratio
                   << std::setprecision(4) << (total_ms / samples_per_ratio) << std::setprecision(2) << "\n";
    }
    return 0;
}
