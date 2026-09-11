#pragma once

#include <string>
#include <vector>

#include "predictors.hpp"

std::vector<Outcome> pattern_always_taken(int n);
std::vector<Outcome> pattern_alternating(int n);
// (period-1) taken outcomes then 1 not-taken, repeating — models a
// for-loop's backward branch (taken every iteration but the last).
std::vector<Outcome> pattern_loop(int n, int period);
std::vector<Outcome> pattern_random(int n, double p_taken, unsigned seed);

// Trace file format: whitespace/newline-separated "T"/"N" tokens
// (case-insensitive).
std::vector<Outcome> load_trace_file(const std::string& path);

std::string outcomes_to_string(const std::vector<Outcome>& outcomes);
