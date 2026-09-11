// Plain assert-based tests, no framework.
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <random>

#include "sat.hpp"

// Re-checks the model against the raw clauses, so a passing test proves
// the returned assignment is actually a valid witness, not just that
// solve() returned true.
static bool verify_model(const std::vector<std::vector<int>>& clauses, const Solver& solver) {
    for (const auto& clause : clauses) {
        bool satisfied = false;
        for (int lit : clause) {
            int var = std::abs(lit);
            bool val = solver.value_of(var);
            if ((lit > 0) == val) {
                satisfied = true;
                break;
            }
        }
        if (!satisfied) return false;
    }
    return true;
}

static void test_sat_formula_from_directive_example() {
    // (x1 OR x2) AND (NOT x1 OR x3) AND (NOT x3 OR x2)
    std::vector<std::vector<int>> clauses = {{1, 2}, {-1, 3}, {-3, 2}};
    Solver solver(3, clauses);
    assert(solver.solve());
    assert(verify_model(clauses, solver));
    std::cout << "ok: SAT formula solved with a valid model\n";
}

static void test_unsat_formula() {
    // (x1 OR x2) AND (x1 OR NOT x2) AND (NOT x1 OR x2) AND (NOT x1 OR NOT x2)
    std::vector<std::vector<int>> clauses = {{1, 2}, {1, -2}, {-1, 2}, {-1, -2}};
    Solver solver(2, clauses);
    assert(!solver.solve());
    std::cout << "ok: UNSAT formula correctly rejected\n";
}

static void test_single_variable_unit_clause() {
    Solver solver(1, {{1}});
    assert(solver.solve());
    assert(solver.value_of(1) == true);
    std::cout << "ok: single-variable unit clause -> x1 = true\n";
}

static void test_contradiction_single_variable() {
    // x1 AND NOT x1
    Solver solver(1, {{1}, {-1}});
    assert(!solver.solve());
    std::cout << "ok: direct contradiction (x1 AND NOT x1) -> UNSAT\n";
}

static void test_dont_care_variable_gets_a_value() {
    // Only x1 is constrained; x2 never appears. solve() must still
    // produce a complete model (no variable left unassigned).
    Solver solver(2, {{1}});
    assert(solver.solve());
    assert(solver.value_of(1) == true);
    (void)solver.value_of(2); // must not crash / must be a concrete bool
    std::cout << "ok: don't-care variable still gets a concrete value\n";
}

static void test_most_occurrences_heuristic_agrees_with_first_unassigned() {
    std::vector<std::vector<int>> clauses = {{1, 2}, {-1, 3}, {-3, 2}};
    Solver a(3, clauses, Heuristic::FirstUnassigned);
    Solver b(3, clauses, Heuristic::MostOccurrences);
    bool sa = a.solve(), sb = b.solve();
    assert(sa == sb);
    assert(sa && verify_model(clauses, a));
    assert(sb && verify_model(clauses, b));
    std::cout << "ok: both heuristics agree on satisfiability\n";
}

static void test_empty_clause_list_is_trivially_satisfiable_edge_case() {
    Solver solver(3, {});
    assert(solver.solve()); // vacuously true: no constraints to violate
    std::cout << "ok: empty clause list is trivially SAT\n";
}

static void test_dimacs_parser_round_trip() {
    CnfFormula f = parse_dimacs("examples/example.cnf");
    assert(f.num_vars == 3);
    assert(f.clauses.size() == 3);
    Solver solver(f.num_vars, f.clauses);
    assert(solver.solve());
    assert(verify_model(f.clauses, solver));
    std::cout << "ok: DIMACS parser loads examples/example.cnf correctly\n";
}

static void test_watched_literals_agrees_with_naive_normal_case() {
    std::vector<std::vector<int>> clauses = {{1, 2}, {-1, 3}, {-3, 2}};
    Solver naive(3, clauses, Heuristic::FirstUnassigned, Propagation::Naive);
    Solver watched(3, clauses, Heuristic::FirstUnassigned, Propagation::WatchedLiterals);
    bool sn = naive.solve(), sw = watched.solve();
    assert(sn == sw);
    assert(sw && verify_model(clauses, watched));
    // Same algorithm, same search order -- watched literals is a faster
    // way to reach the identical fixpoint, not a different search.
    assert(naive.stats().decisions == watched.stats().decisions);
    assert(naive.stats().backtracks == watched.stats().backtracks);
    std::cout << "ok: watched-literals propagation agrees with naive (SAT + identical decisions/backtracks)\n";
}

static void test_watched_literals_agrees_with_naive_on_unsat_invalid_case() {
    std::vector<std::vector<int>> clauses = {{1, 2}, {1, -2}, {-1, 2}, {-1, -2}};
    Solver naive(2, clauses, Heuristic::FirstUnassigned, Propagation::Naive);
    Solver watched(2, clauses, Heuristic::FirstUnassigned, Propagation::WatchedLiterals);
    assert(naive.solve() == false);
    assert(watched.solve() == false);
    assert(naive.stats().decisions == watched.stats().decisions);
    assert(naive.stats().backtracks == watched.stats().backtracks);
    std::cout << "ok: watched-literals propagation agrees with naive on UNSAT too\n";
}

static void test_watched_literals_handles_unit_clause_normal_case() {
    Solver solver(1, {{1}}, Heuristic::FirstUnassigned, Propagation::WatchedLiterals);
    assert(solver.solve());
    assert(solver.value_of(1) == true);
    std::cout << "ok: watched-literals forces an original unit clause correctly\n";
}

static void test_watched_literals_detects_contradicting_unit_clauses_invalid_case() {
    // x1 AND NOT x1, both length-1 clauses -- init_watches() must catch
    // this itself (the general watch mechanism can't: a length-1 clause
    // has no "other" watched literal to fall back on).
    Solver solver(1, {{1}, {-1}}, Heuristic::FirstUnassigned, Propagation::WatchedLiterals);
    assert(!solver.solve());
    std::cout << "ok: watched-literals rejects two directly-contradicting unit clauses\n";
}

static void test_watched_literals_empty_formula_edge_case() {
    Solver solver(3, {}, Heuristic::FirstUnassigned, Propagation::WatchedLiterals);
    assert(solver.solve());
    std::cout << "ok: watched-literals handles an empty clause list\n";
}

static void test_vsids_heuristic_agrees_with_others_normal_case() {
    std::vector<std::vector<int>> clauses = {{1, 2}, {-1, 3}, {-3, 2}};
    Solver a(3, clauses, Heuristic::FirstUnassigned);
    Solver v(3, clauses, Heuristic::Vsids);
    bool sa = a.solve(), sv = v.solve();
    assert(sa == sv);
    assert(sv && verify_model(clauses, v));
    std::cout << "ok: VSIDS heuristic agrees with first-unassigned on satisfiability\n";
}

static void test_vsids_finds_unsat_invalid_case() {
    std::vector<std::vector<int>> clauses = {{1, 2}, {1, -2}, {-1, 2}, {-1, -2}};
    Solver v(2, clauses, Heuristic::Vsids);
    assert(!v.solve());
    std::cout << "ok: VSIDS heuristic correctly finds UNSAT\n";
}

static void test_watched_literals_matches_naive_across_random_instances_regression() {
    std::mt19937 rng(7);
    std::uniform_int_distribution<int> var_dist(1, 12);
    std::uniform_int_distribution<int> sign_dist(0, 1);
    for (int trial = 0; trial < 30; trial++) {
        std::vector<std::vector<int>> clauses;
        for (int c = 0; c < 50; c++) {
            std::vector<int> clause;
            while ((int)clause.size() < 3) {
                int v = var_dist(rng);
                bool already = false;
                for (int lit : clause)
                    if (std::abs(lit) == v) already = true;
                if (already) continue;
                clause.push_back(sign_dist(rng) ? v : -v);
            }
            clauses.push_back(clause);
        }
        Solver naive(12, clauses, Heuristic::FirstUnassigned, Propagation::Naive);
        Solver watched(12, clauses, Heuristic::FirstUnassigned, Propagation::WatchedLiterals);
        bool sn = naive.solve(), sw = watched.solve();
        assert(sn == sw);
        assert(naive.stats().decisions == watched.stats().decisions);
        assert(naive.stats().backtracks == watched.stats().backtracks);
        if (sn) assert(verify_model(clauses, naive));
    }
    std::cout << "ok: watched-literals matches naive across 30 random instances (SAT/UNSAT, decisions, backtracks)\n";
}

static void test_dimacs_parser_invalid_file_invalid_case() {
    bool threw = false;
    try {
        parse_dimacs("examples/does-not-exist.cnf");
    } catch (const std::exception&) {
        threw = true;
    }
    assert(threw);
    std::cout << "ok: missing DIMACS file raises an error\n";
}

int main() {
    test_sat_formula_from_directive_example();
    test_unsat_formula();
    test_single_variable_unit_clause();
    test_contradiction_single_variable();
    test_dont_care_variable_gets_a_value();
    test_most_occurrences_heuristic_agrees_with_first_unassigned();
    test_empty_clause_list_is_trivially_satisfiable_edge_case();
    test_dimacs_parser_round_trip();
    test_dimacs_parser_invalid_file_invalid_case();
    test_watched_literals_agrees_with_naive_normal_case();
    test_watched_literals_agrees_with_naive_on_unsat_invalid_case();
    test_watched_literals_handles_unit_clause_normal_case();
    test_watched_literals_detects_contradicting_unit_clauses_invalid_case();
    test_watched_literals_empty_formula_edge_case();
    test_vsids_heuristic_agrees_with_others_normal_case();
    test_vsids_finds_unsat_invalid_case();
    test_watched_literals_matches_naive_across_random_instances_regression();
    std::cout << "all tests passed\n";
    return 0;
}
