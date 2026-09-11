#pragma once

#include <array>
#include <string>
#include <vector>

// DPLL SAT solver over CNF formulas.
//
// A literal is a non-zero int: +v means variable v, -v means NOT
// variable v (variables are numbered 1..num_vars, DIMACS-style). A
// clause is a disjunction (OR) of literals; a formula is a conjunction
// (AND) of clauses.

// MostOccurrences is a *static* heuristic: it counts clause occurrences
// once per decision, unaffected by which clauses have caused conflicts.
// Vsids is dynamic: every clause that ever causes a propagation conflict
// bumps its variables' "activity" score, and all activity decays each
// time (so *recent* conflict involvement outweighs old), then branches
// on the currently-highest-activity unassigned variable -- a scaled-down
// version of the heuristic every competitive CDCL solver actually uses.
enum class Heuristic { FirstUnassigned, MostOccurrences, Vsids };

// Naive: rescans every clause on every propagate() call, O(clauses x
// literals) per pass -- see propagate_naive()'s own comment for why
// that's the right trade-off for a lab this size. WatchedLiterals:
// the two-watched-literals scheme every real solver uses -- each
// clause tracks only 2 of its literals; propagate() only re-examines a
// clause when one of those 2 becomes false, not on every pass over
// every clause. Same algorithm (same decisions, same backtracks -- see
// the regression test), just a faster way to reach the same fixpoint.
enum class Propagation { Naive, WatchedLiterals };

struct SolveStats {
    long long decisions = 0;
    long long backtracks = 0;
    long long unit_propagations = 0;
    double runtime_ms = 0.0;
};

class Solver {
public:
    Solver(int num_vars, std::vector<std::vector<int>> clauses, Heuristic heuristic = Heuristic::FirstUnassigned,
           Propagation propagation = Propagation::Naive);

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
    Propagation propagation_;
    std::vector<int> assignment_; // 0=unassigned, 1=true, 2=false; index 1..num_vars_
    std::vector<double> activity_; // VSIDS score, index 1..num_vars_
    SolveStats stats_;

    // Watched-literals state (Propagation::WatchedLiterals only).
    std::vector<std::array<int, 2>> watch_idx_; // per clause: indices (into that clause's vector) of its 2 watched literals
    std::vector<std::vector<int>> watch_lists_; // watch_lists_[lit_slot(l)] = clause indices currently watching literal l

    bool dpll(int forced_lit = 0);
    bool propagate(std::vector<int>& trail, int forced_lit);
    bool propagate_naive(std::vector<int>& trail);
    bool propagate_watched(std::vector<int>& trail, int forced_lit);
    bool init_watches(); // one-time setup + forces the formula's original unit clauses
    int lit_slot(int lit) const { return lit > 0 ? lit : num_vars_ - lit; }
    bool is_true(int lit) const;
    bool is_false(int lit) const;
    void bump_activity(const std::vector<int>& conflicting_clause);
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
