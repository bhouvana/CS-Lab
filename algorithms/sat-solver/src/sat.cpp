// CNF -> DPLL -> SAT assignment (or UNSAT)
#include "sat.hpp"

#include <chrono>
#include <cstdlib>

namespace {
constexpr int UNASSIGNED = 0;
constexpr int VAL_TRUE = 1;
constexpr int VAL_FALSE = 2;
} // namespace

Solver::Solver(int num_vars, std::vector<std::vector<int>> clauses, Heuristic heuristic, Propagation propagation)
    : num_vars_(num_vars), clauses_(std::move(clauses)), heuristic_(heuristic), propagation_(propagation),
      assignment_(num_vars + 1, UNASSIGNED), activity_(num_vars + 1, 0.0) {}

bool Solver::value_of(int var) const {
    return assignment_[var] == VAL_TRUE;
}

bool Solver::is_true(int lit) const {
    int var = std::abs(lit);
    return assignment_[var] != UNASSIGNED && (assignment_[var] == VAL_TRUE) == (lit > 0);
}

bool Solver::is_false(int lit) const {
    int var = std::abs(lit);
    return assignment_[var] != UNASSIGNED && (assignment_[var] == VAL_TRUE) != (lit > 0);
}

void Solver::bump_activity(const std::vector<int>& conflicting_clause) {
    for (int lit : conflicting_clause) activity_[std::abs(lit)] += 1.0;
    // Decay everything so recent conflicts outweigh old ones -- without
    // this, a variable involved in one early conflict and never again
    // would still outrank a variable involved in every recent conflict.
    for (double& a : activity_) a *= 0.95;
}

bool Solver::propagate(std::vector<int>& trail, int forced_lit) {
    if (propagation_ == Propagation::WatchedLiterals) return propagate_watched(trail, forced_lit);
    return propagate_naive(trail); // rescans everything regardless of what just changed
}

// Naive fixed-point unit propagation: repeatedly scan every clause for
// one that's unresolved with exactly one unassigned literal remaining
// (a "unit clause"), and force that literal true. Returns false the
// moment a clause has zero unassigned literals and isn't satisfied
// (a conflict). O(clauses x literals) per pass — see propagate_watched
// for the faster alternative, and CS-LAB.md §6 for why this was the
// original, and still default, trade-off for a lab this size.
bool Solver::propagate_naive(std::vector<int>& trail) {
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
            if (unassigned_count == 0) {
                if (heuristic_ == Heuristic::Vsids) bump_activity(clause);
                return false; // every literal false: conflict
            }
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

// Two-watched-literals: each clause watches only 2 of its literals.
// When a literal `lit` becomes true, only clauses watching `-lit` (the
// literal that just became *false*) need re-examining -- every other
// clause's watched pair is untouched, so most clauses are never looked
// at during most propagations. For each affected clause, try to find a
// replacement literal to watch instead (any literal that isn't false);
// if none exists, the clause is unit (force the other watched literal)
// or conflicting (both watched literals false).
//
// `forced_lit` seeds the initial queue with the one literal that just
// became true (the decision dpll() made right before calling this) --
// 0 at the very top of the search, where init_watches() has already
// fully propagated the formula's original unit clauses.
bool Solver::propagate_watched(std::vector<int>& trail, int forced_lit) {
    std::vector<int> queue;
    if (forced_lit != 0) queue.push_back(forced_lit);

    while (!queue.empty()) {
        int lit = queue.back();
        queue.pop_back();
        int neg = -lit;
        int slot = lit_slot(neg);

        std::vector<int> current;
        current.swap(watch_lists_[slot]); // rebuilt below as we go

        for (size_t idx = 0; idx < current.size(); idx++) {
            int ci = current[idx];
            auto& cl = clauses_[ci];
            auto& w = watch_idx_[ci];
            int pos = (cl[w[0]] == neg) ? 0 : 1;
            int other_pos = 1 - pos;
            int other_lit = cl[w[other_pos]];

            if (is_true(other_lit)) {
                watch_lists_[slot].push_back(ci); // already satisfied, keep watching neg unchanged
                continue;
            }

            int replacement = -1;
            for (size_t k = 0; k < cl.size(); k++) {
                if ((int)k == w[other_pos]) continue;
                if (!is_false(cl[k])) {
                    replacement = (int)k;
                    break;
                }
            }
            if (replacement != -1) {
                w[pos] = replacement;
                watch_lists_[lit_slot(cl[replacement])].push_back(ci); // moved to a new literal's list
                continue;
            }

            if (is_false(other_lit)) {
                if (heuristic_ == Heuristic::Vsids) bump_activity(cl);
                // Conflict: restore this clause and every not-yet-processed
                // one back onto neg's list before returning -- they never
                // stopped watching neg, we just didn't get to them.
                watch_lists_[slot].push_back(ci);
                for (size_t j = idx + 1; j < current.size(); j++) watch_lists_[slot].push_back(current[j]);
                return false;
            }

            // No replacement, other watched literal unassigned: unit clause.
            watch_lists_[slot].push_back(ci); // still watches neg
            int forced_var = std::abs(other_lit);
            assignment_[forced_var] = (other_lit > 0) ? VAL_TRUE : VAL_FALSE;
            trail.push_back(forced_var);
            stats_.unit_propagations++;
            queue.push_back(other_lit);
        }
    }
    return true;
}

// One-time setup for Propagation::WatchedLiterals: assigns each clause
// its initial 2 watched literals, and forces the formula's original
// unit clauses directly (a length-1 clause has no "other" literal to
// fall back on, so the general watched mechanism above can't detect it
// as unit on its own -- exactly like a real solver's initial BCP pass).
bool Solver::init_watches() {
    watch_lists_.assign(2 * num_vars_ + 1, {});
    watch_idx_.assign(clauses_.size(), {0, 0});

    std::vector<int> forced;
    for (size_t ci = 0; ci < clauses_.size(); ci++) {
        auto& cl = clauses_[ci];
        if (cl.size() == 1) {
            int lit = cl[0];
            int var = std::abs(lit);
            int want = (lit > 0) ? VAL_TRUE : VAL_FALSE;
            if (assignment_[var] == UNASSIGNED) {
                assignment_[var] = want;
                forced.push_back(lit);
            } else if (assignment_[var] != want) {
                return false; // two original unit clauses directly contradict
            }
            continue; // resolved once and for all -- no watches needed
        }
        watch_idx_[ci] = {0, 1};
        watch_lists_[lit_slot(cl[0])].push_back((int)ci);
        watch_lists_[lit_slot(cl[1])].push_back((int)ci);
    }

    std::vector<int> trail;
    for (int lit : forced) {
        if (!propagate_watched(trail, lit)) return false;
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

    if (heuristic_ == Heuristic::Vsids) {
        // Highest-activity unassigned variable: the one most involved in
        // *recent* conflicts (see bump_activity's decay), a proxy for
        // "most likely to matter" without maintaining occurrence counts.
        int best = 0;
        for (int v = 1; v <= num_vars_; v++)
            if (assignment_[v] == UNASSIGNED && (best == 0 || activity_[v] > activity_[best])) best = v;
        return best;
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

bool Solver::dpll(int forced_lit) {
    std::vector<int> trail;

    if (!propagate(trail, forced_lit)) {
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
    if (dpll(var)) return true;
    stats_.backtracks++;

    assignment_[var] = VAL_FALSE;
    if (dpll(-var)) return true;
    stats_.backtracks++;

    assignment_[var] = UNASSIGNED;
    for (int v : trail) assignment_[v] = UNASSIGNED;
    return false;
}

bool Solver::solve() {
    auto start = std::chrono::steady_clock::now();
    bool sat = (propagation_ == Propagation::WatchedLiterals) ? (init_watches() && dpll()) : dpll();
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
