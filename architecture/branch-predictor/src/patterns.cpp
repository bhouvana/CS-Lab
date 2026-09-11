#include "patterns.hpp"

#include <cctype>
#include <fstream>
#include <random>
#include <stdexcept>

std::vector<Outcome> pattern_always_taken(int n) {
    return std::vector<Outcome>(n, Outcome::Taken);
}

std::vector<Outcome> pattern_alternating(int n) {
    std::vector<Outcome> out(n);
    for (int i = 0; i < n; i++) out[i] = (i % 2 == 0) ? Outcome::Taken : Outcome::NotTaken;
    return out;
}

std::vector<Outcome> pattern_loop(int n, int period) {
    std::vector<Outcome> out(n);
    for (int i = 0; i < n; i++) out[i] = ((i % period) == period - 1) ? Outcome::NotTaken : Outcome::Taken;
    return out;
}

std::vector<Outcome> pattern_random(int n, double p_taken, unsigned seed) {
    std::mt19937 rng(seed);
    std::bernoulli_distribution dist(p_taken);
    std::vector<Outcome> out(n);
    for (int i = 0; i < n; i++) out[i] = dist(rng) ? Outcome::Taken : Outcome::NotTaken;
    return out;
}

std::vector<Outcome> load_trace_file(const std::string& path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("cannot open " + path);
    std::vector<Outcome> out;
    std::string tok;
    while (in >> tok) {
        char c = (char)std::toupper((unsigned char)tok[0]);
        if (c == 'T') out.push_back(Outcome::Taken);
        else if (c == 'N') out.push_back(Outcome::NotTaken);
        else throw std::runtime_error("unrecognized token '" + tok + "' in " + path + " (expected T or N)");
    }
    return out;
}

std::string outcomes_to_string(const std::vector<Outcome>& outcomes) {
    std::string s;
    for (size_t i = 0; i < outcomes.size(); i++) {
        if (i) s += ' ';
        s += (outcomes[i] == Outcome::Taken) ? 'T' : 'N';
    }
    return s;
}
