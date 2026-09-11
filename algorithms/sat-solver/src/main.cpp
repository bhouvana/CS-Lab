// CLI: sat <file.cnf> [--heuristic first|most|vsids] [--propagation naive|watched]
#include <iostream>
#include <string>

#include "sat.hpp"

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: " << argv[0]
                   << " <file.cnf> [--heuristic first|most|vsids] [--propagation naive|watched]\n";
        return 2;
    }

    Heuristic heuristic = Heuristic::FirstUnassigned;
    Propagation propagation = Propagation::Naive;
    for (int i = 2; i + 1 < argc; i += 2) {
        std::string flag = argv[i];
        std::string value = argv[i + 1];
        if (flag == "--heuristic") {
            if (value == "most") heuristic = Heuristic::MostOccurrences;
            else if (value == "vsids") heuristic = Heuristic::Vsids;
            else if (value != "first") {
                std::cerr << "unknown heuristic '" << value << "', expected first|most|vsids\n";
                return 2;
            }
        } else if (flag == "--propagation") {
            if (value == "watched") propagation = Propagation::WatchedLiterals;
            else if (value != "naive") {
                std::cerr << "unknown propagation '" << value << "', expected naive|watched\n";
                return 2;
            }
        } else {
            std::cerr << "unknown flag '" << flag << "'\n";
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

    Solver solver(formula.num_vars, formula.clauses, heuristic, propagation);
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
