#pragma once

#include <string>
#include <vector>

// DPLL SAT solver over CNF formulas.
//
// A literal is a non-zero int: +v means variable v, -v means NOT
// variable v (variables are numbered 1..num_vars, DIMACS-style). A
// clause is a disjunction (OR) of literals; a formula is a conjunction
// (AND) of clauses.

enum class Heuristic { FirstUnassigned, MostOccurrences };

struct SolveStats {
    long long decisions = 0;
    long long backtracks = 0;
    long long unit_propagations = 0;
    double runtime_ms = 0.0;
};

class Solver {
public:
    Solver(int num_vars, std::vector<std::vector<int>> clauses, Heuristic heuristic = Heuristic::FirstUnassigned);

    // Runs DPLL (unit propagation + branch-and-backtrack). Returns true
    // if satisfiable. On success, value_of() gives a satisfying model
    // for every variable (variables that don't affect satisfiability
    // are arbitrarily fixed to true, so the model is always complete).
    bool solve();

    bool value_of(int var) const; // only meaningful after solve() returns true
    const SolveStats& stats() const { return stats_; }
    int num_vars() const { return num_vars_; }
    size_t num_clauses() const { return clauses_.size(); }

private:
    int num_vars_;
    std::vector<std::vector<int>> clauses_;
    Heuristic heuristic_;
    std::vector<int> assignment_; // 0=unassigned, 1=true, 2=false; index 1..num_vars_
    SolveStats stats_;

    bool dpll();
    bool propagate(std::vector<int>& trail);
    bool all_satisfied() const;
    int pick_branch_variable() const;
};

struct CnfFormula {
    int num_vars = 0;
    std::vector<std::vector<int>> clauses;
};

// Parses a DIMACS CNF file: lines starting with 'c' are comments, a
// line "p cnf <num_vars> <num_clauses>" is the header, and each clause
// is a whitespace/newline-separated list of literals terminated by a
// literal 0. Throws std::runtime_error on malformed input.
CnfFormula parse_dimacs(const std::string& path);
