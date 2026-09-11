// Plain assert-based tests, no framework.
#include <cassert>
#include <cstdio>
#include <fstream>
#include <iostream>

#include "patterns.hpp"
#include "predictors.hpp"

static void test_always_taken_on_all_taken_normal_case() {
    AlwaysTaken p;
    auto trace = pattern_always_taken(50);
    assert(evaluate_predictor(p, trace) == 1.0);
    std::cout << "ok: always-taken is 100% accurate on an all-taken trace\n";
}

static void test_always_taken_on_alternating_is_50_percent() {
    AlwaysTaken p;
    auto trace = pattern_alternating(100); // half taken, half not
    assert(evaluate_predictor(p, trace) == 0.5);
    std::cout << "ok: always-taken is 50% accurate on an alternating trace\n";
}

static void test_one_bit_fails_completely_on_alternating_edge_case() {
    // Strictly alternating is the textbook worst case for a predictor
    // that just repeats the last outcome: it is always exactly wrong.
    OneBitPredictor p;
    auto trace = pattern_alternating(100);
    double acc = evaluate_predictor(p, trace);
    assert(acc < 0.02); // first prediction may accidentally match; rest never do
    std::cout << "ok: 1-bit predictor is ~0% accurate on alternating (its worst case)\n";
}

static void test_two_bit_beats_one_bit_on_loop_pattern() {
    // A loop pattern (mostly taken, occasional single not-taken) is
    // exactly what 2-bit counters are designed to tolerate better than
    // 1-bit: a single glitch shouldn't flip the prediction.
    OneBitPredictor one_bit;
    TwoBitPredictor two_bit;
    auto trace = pattern_loop(1000, 10); // 9 taken, 1 not-taken, repeating
    double one_bit_acc = evaluate_predictor(one_bit, trace);
    double two_bit_acc = evaluate_predictor(two_bit, trace);
    assert(two_bit_acc > one_bit_acc);
    std::cout << "ok: 2-bit (" << two_bit_acc << ") beats 1-bit (" << one_bit_acc << ") on a loop pattern\n";
}

static void test_gshare_learns_alternating_pattern() {
    // Unlike 1-bit/2-bit, gshare's prediction is indexed by recent
    // history, so it can eventually learn "after T comes N and vice
    // versa" — a fixed, perfectly periodic pattern.
    GSharePredictor gshare;
    auto trace = pattern_alternating(1000);
    double acc = evaluate_predictor(gshare, trace);
    assert(acc > 0.9); // near-perfect once warmed up
    std::cout << "ok: gshare learns alternating pattern -> " << acc << " accuracy\n";
}

static void test_empty_trace_edge_case() {
    AlwaysTaken p;
    std::vector<Outcome> empty;
    assert(evaluate_predictor(p, empty) == 0.0); // defined as 0, not NaN/crash
    std::cout << "ok: empty trace handled without crashing\n";
}

static void test_load_trace_file_normal_case() {
    auto trace = load_trace_file("examples/alternating.trace");
    assert(!trace.empty());
    assert(trace[0] == Outcome::Taken);
    assert(trace[1] == Outcome::NotTaken);
    std::cout << "ok: load_trace_file parses examples/alternating.trace\n";
}

static void test_load_trace_file_missing_file_invalid_case() {
    bool threw = false;
    try {
        load_trace_file("examples/does-not-exist.trace");
    } catch (const std::exception&) {
        threw = true;
    }
    assert(threw);
    std::cout << "ok: missing trace file raises an error\n";
}

static void test_load_trace_file_bad_token_invalid_case() {
    // Write a small malformed trace, parse it, then clean up.
    {
        std::ofstream out("tests/tmp_bad.trace");
        out << "T N X T\n";
    }
    bool threw = false;
    try {
        load_trace_file("tests/tmp_bad.trace");
    } catch (const std::exception&) {
        threw = true;
    }
    std::remove("tests/tmp_bad.trace");
    assert(threw);
    std::cout << "ok: unrecognized token in a trace file raises an error\n";
}

int main() {
    test_always_taken_on_all_taken_normal_case();
    test_always_taken_on_alternating_is_50_percent();
    test_one_bit_fails_completely_on_alternating_edge_case();
    test_two_bit_beats_one_bit_on_loop_pattern();
    test_gshare_learns_alternating_pattern();
    test_empty_trace_edge_case();
    test_load_trace_file_normal_case();
    test_load_trace_file_missing_file_invalid_case();
    test_load_trace_file_bad_token_invalid_case();
    std::cout << "all tests passed\n";
    return 0;
}
