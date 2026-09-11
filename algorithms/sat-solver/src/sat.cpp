// CNF -> DPLL -> SAT assignment (or UNSAT)
#include "sat.hpp"

#include <chrono>
#include <cstdlib>

namespace {
constexpr int UNASSIGNED = 0;
constexpr int VAL_TRUE = 1;
constexpr int VAL_FALSE = 2;
} // namespace

Solver::Solver(int num_vars, std::vector<std::vector<int>> clauses, Heuristic heuristic)
    : num_vars_(num_vars), clauses_(std::move(clauses)), heuristic_(heuristic),
      assignment_(num_vars + 1, UNASSIGNED) {}

bool Solver::value_of(int var) const {
    return assignment_[var] == VAL_TRUE;
}

// Naive fixed-point unit propagation: repeatedly scan every clause for
// one that's unresolved with exactly one unassigned literal remaining
// (a "unit clause"), and force that literal true. Returns false the
// moment a clause has zero unassigned literals and isn't satisfied
// (a conflict). Not watched-literals — O(clauses x literals) per pass —
// which is the right trade for a lab this size (see CS-LAB.md §6).
bool Solver::propagate(std::vector<int>& trail) {
    bool changed = true;
    while (changed) {
        changed = false;
        for (const auto& clause : clauses_) {
            bool satisfied = false;
            int unassigned_lit = 0;
            int unassigned_count = 0;

            for (int lit : clause) {
                int var = std::abs(lit);
                int val = assignment_[var];
                if (val == UNASSIGNED) {
                    unassigned_count++;
                    unassigned_lit = lit;
                } else {
                    bool lit_is_true = (val == VAL_TRUE) == (lit > 0);
                    if (lit_is_true) {
                        satisfied = true;
                        break;
                    }
                }
            }

            if (satisfied) continue;
            if (unassigned_count == 0) return false; // every literal false: conflict
            if (unassigned_count == 1) {
                int var = std::abs(unassigned_lit);
                assignment_[var] = (unassigned_lit > 0) ? VAL_TRUE : VAL_FALSE;
                trail.push_back(var);
                stats_.unit_propagations++;
                changed = true;
            }
        }
    }
    return true;
}

bool Solver::all_satisfied() const {
    for (const auto& clause : clauses_) {
        bool satisfied = false;
        for (int lit : clause) {
            int var = std::abs(lit);
            if (assignment_[var] == UNASSIGNED) continue;
            bool lit_is_true = (assignment_[var] == VAL_TRUE) == (lit > 0);
            if (lit_is_true) {
                satisfied = true;
                break;
            }
        }
        if (!satisfied) return false;
    }
    return true;
}

int Solver::pick_branch_variable() const {
    if (heuristic_ == Heuristic::FirstUnassigned) {
        for (int v = 1; v <= num_vars_; v++)
            if (assignment_[v] == UNASSIGNED) return v;
        return 0;
    }

    // MostOccurrences: pick the currently-unassigned variable appearing
    // in the most clauses. Recomputed on every decision rather than
    // tracked incrementally — simpler, and cheap enough at lab scale.
    std::vector<int> occurrences(num_vars_ + 1, 0);
    for (const auto& clause : clauses_)
        for (int lit : clause) {
            int var = std::abs(lit);
            if (assignment_[var] == UNASSIGNED) occurrences[var]++;
        }

    int best = 0;
    for (int v = 1; v <= num_vars_; v++)
        if (assignment_[v] == UNASSIGNED && (best == 0 || occurrences[v] > occurrences[best])) best = v;
    return best;
}

bool Solver::dpll() {
    std::vector<int> trail;

    if (!propagate(trail)) {
        for (int v : trail) assignment_[v] = UNASSIGNED;
        return false;
    }

    if (all_satisfied()) return true;

    int var = pick_branch_variable();
    if (var == 0) {
        // No unassigned variable left, yet not all clauses satisfied:
        // shouldn't happen (propagate() would have flagged the conflict),
        // but treated as UNSAT defensively rather than looping forever.
        for (int v : trail) assignment_[v] = UNASSIGNED;
        return false;
    }

    stats_.decisions++;
    assignment_[var] = VAL_TRUE;
    if (dpll()) return true;
    stats_.backtracks++;

    assignment_[var] = VAL_FALSE;
    if (dpll()) return true;
    stats_.backtracks++;

    assignment_[var] = UNASSIGNED;
    for (int v : trail) assignment_[v] = UNASSIGNED;
    return false;
}

bool Solver::solve() {
    auto start = std::chrono::steady_clock::now();
    bool sat = dpll();
    auto end = std::chrono::steady_clock::now();
    stats_.runtime_ms = std::chrono::duration<double, std::milli>(end - start).count();

    if (sat) {
        // Variables that never affected satisfiability may still be
        // UNASSIGNED ("don't care") — fix them to a concrete value so
        // callers always get a complete model.
        for (int v = 1; v <= num_vars_; v++)
            if (assignment_[v] == UNASSIGNED) assignment_[v] = VAL_TRUE;
    }
    return sat;
}
