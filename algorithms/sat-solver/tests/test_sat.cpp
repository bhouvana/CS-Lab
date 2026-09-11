// Plain assert-based tests, no framework.
#include <cassert>
#include <iostream>

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
    std::cout << "all tests passed\n";
    return 0;
}
