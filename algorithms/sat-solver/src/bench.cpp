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

    // --- Watched literals: same decisions/backtracks, faster wall-clock? ---
    std::cout << "\nBenchmark: naive vs. watched-literals propagation, at the hardest ratio (4.27)\n";
    std::cout << "(20 variables, same instances fed to both -- decisions/backtracks should match exactly)\n\n";
    std::cout << std::left << std::setw(10) << "clauses" << std::setw(16) << "naive ms" << std::setw(20)
               << "watched ms" << std::setw(12) << "speedup" << std::setw(24) << "decisions match?" << "\n";
    {
        std::mt19937 rng2(42);
        int num_clauses = (int)(4.27 * 20);
        double naive_total = 0.0, watched_total = 0.0;
        bool all_match = true;
        for (int s = 0; s < samples_per_ratio; s++) {
            auto clauses = random_3sat(20, num_clauses, rng2);
            Solver naive(20, clauses, Heuristic::FirstUnassigned, Propagation::Naive);
            Solver watched(20, clauses, Heuristic::FirstUnassigned, Propagation::WatchedLiterals);
            bool sn = naive.solve(), sw = watched.solve();
            naive_total += naive.stats().runtime_ms;
            watched_total += watched.stats().runtime_ms;
            if (sn != sw || naive.stats().decisions != watched.stats().decisions ||
                naive.stats().backtracks != watched.stats().backtracks)
                all_match = false;
        }
        double speedup = watched_total > 0.0 ? naive_total / watched_total : 0.0;
        std::cout << std::left << std::setw(10) << num_clauses << std::setw(16) << std::setprecision(4)
                   << naive_total << std::setw(20) << watched_total << std::setw(12) << std::setprecision(2)
                   << speedup << std::setw(24) << (all_match ? "yes, all 20" : "MISMATCH") << "\n";
    }

    // --- VSIDS vs most-occurrences, at the hardest ratio ---
    std::cout << "\nBenchmark: VSIDS vs. most-occurrences heuristic, at the hardest ratio (4.27)\n";
    std::cout << "(same instances fed to both)\n\n";
    std::cout << std::left << std::setw(10) << "clauses" << std::setw(20) << "most-occ decisions" << std::setw(20)
               << "vsids decisions" << std::setw(20) << "most-occ backtracks" << "vsids backtracks" << "\n";
    {
        std::mt19937 rng3(42);
        int num_clauses = (int)(4.27 * 20);
        long long mo_dec = 0, mo_bt = 0, vs_dec = 0, vs_bt = 0;
        for (int s = 0; s < samples_per_ratio; s++) {
            auto clauses = random_3sat(20, num_clauses, rng3);
            Solver mo(20, clauses, Heuristic::MostOccurrences);
            Solver vs(20, clauses, Heuristic::Vsids);
            mo.solve();
            vs.solve();
            mo_dec += mo.stats().decisions;
            mo_bt += mo.stats().backtracks;
            vs_dec += vs.stats().decisions;
            vs_bt += vs.stats().backtracks;
        }
        std::cout << std::left << std::setw(10) << num_clauses << std::setw(20)
                   << (double)mo_dec / samples_per_ratio << std::setw(20) << (double)vs_dec / samples_per_ratio
                   << std::setw(20) << (double)mo_bt / samples_per_ratio << (double)vs_bt / samples_per_ratio
                   << "\n";
    }

    // --- Does backtrack count at the hardest ratio scale with problem size? ---
    std::cout << "\nBenchmark: backtrack count vs. num_vars, at the hardest ratio (4.27)\n\n";
    std::cout << std::left << std::setw(10) << "num_vars" << std::setw(10) << "clauses" << std::setw(16)
               << "avg decisions" << std::setw(16) << "avg backtracks" << "avg ms" << "\n";
    {
        std::mt19937 rng4(42);
        for (int nv : {10, 14, 18, 22, 26}) {
            int num_clauses = (int)(4.27 * nv);
            long long total_decisions = 0, total_backtracks = 0;
            double total_ms = 0.0;
            const int samples = 10; // fewer than the ratio sweep -- larger nv gets noticeably slower
            for (int s = 0; s < samples; s++) {
                auto clauses = random_3sat(nv, num_clauses, rng4);
                Solver solver(nv, clauses);
                solver.solve();
                total_decisions += solver.stats().decisions;
                total_backtracks += solver.stats().backtracks;
                total_ms += solver.stats().runtime_ms;
            }
            std::cout << std::left << std::setw(10) << nv << std::setw(10) << num_clauses << std::setw(16)
                       << (double)total_decisions / samples << std::setw(16) << (double)total_backtracks / samples
                       << std::setprecision(4) << (total_ms / samples) << std::setprecision(2) << "\n";
        }
    }
    return 0;
}
