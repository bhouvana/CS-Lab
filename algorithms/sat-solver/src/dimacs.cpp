// DIMACS CNF text -> CnfFormula
#include "sat.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>

CnfFormula parse_dimacs(const std::string& path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("cannot open " + path);

    CnfFormula formula;
    int expected_vars = 0, expected_clauses = 0;
    bool saw_header = false;
    std::vector<int> current_clause;

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == 'c') continue; // comment or blank

        if (line[0] == 'p') {
            std::istringstream iss(line);
            std::string p, cnf;
            iss >> p >> cnf >> expected_vars >> expected_clauses;
            if (cnf != "cnf") throw std::runtime_error("expected 'p cnf', got 'p " + cnf + "'");
            formula.num_vars = expected_vars;
            saw_header = true;
            continue;
        }

        if (!saw_header) throw std::runtime_error("clause data before 'p cnf' header in " + path);

        std::istringstream iss(line);
        int lit;
        while (iss >> lit) {
            if (lit == 0) {
                formula.clauses.push_back(current_clause);
                current_clause.clear();
            } else {
                current_clause.push_back(lit);
            }
        }
    }

    if (!current_clause.empty()) throw std::runtime_error("clause missing terminating 0 in " + path);
    if ((int)formula.clauses.size() != expected_clauses) {
        throw std::runtime_error("header declared " + std::to_string(expected_clauses) + " clauses, found " +
                                  std::to_string(formula.clauses.size()));
    }
    return formula;
}
