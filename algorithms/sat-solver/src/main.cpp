// CLI: sat <file.cnf> [--heuristic first|most]
#include <cstring>
#include <iostream>

#include "sat.hpp"

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: " << argv[0] << " <file.cnf> [--heuristic first|most]\n";
        return 2;
    }

    Heuristic heuristic = Heuristic::FirstUnassigned;
    if (argc >= 4 && std::strcmp(argv[2], "--heuristic") == 0) {
        if (std::strcmp(argv[3], "most") == 0) heuristic = Heuristic::MostOccurrences;
        else if (std::strcmp(argv[3], "first") != 0) {
            std::cerr << "unknown heuristic '" << argv[3] << "', expected first|most\n";
            return 2;
        }
    }

    CnfFormula formula;
    try {
        formula = parse_dimacs(argv[1]);
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }

    Solver solver(formula.num_vars, formula.clauses, heuristic);
    bool sat = solver.solve();

    if (sat) {
        std::cout << "SAT\n\n";
        for (int v = 1; v <= solver.num_vars(); v++)
            std::cout << "x" << v << " = " << (solver.value_of(v) ? "true" : "false") << "\n";
    } else {
        std::cout << "UNSAT\n";
    }

    const SolveStats& s = solver.stats();
    std::cout << "\n--- stats ---\n";
    std::cout << "variables:         " << solver.num_vars() << "\n";
    std::cout << "clauses:           " << solver.num_clauses() << "\n";
    std::cout << "decisions:         " << s.decisions << "\n";
    std::cout << "backtracks:        " << s.backtracks << "\n";
    std::cout << "unit propagations: " << s.unit_propagations << "\n";
    std::cout << "runtime:           " << s.runtime_ms << " ms\n";

    return 0;
}
