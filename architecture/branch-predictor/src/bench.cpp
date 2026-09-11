// Experiment: which predictor handles which branch pattern best?
#include <iomanip>
#include <iostream>
#include <cstdint>
#include <vector>

#include "patterns.hpp"
#include "predictors.hpp"

namespace {
class TournamentPredictor : public Predictor {
public:
    Outcome predict() const override {
        return selector_ >= 2 ? gshare_.predict() : one_bit_.predict();
    }

    void update(Outcome actual) override {
        Outcome one_bit_prediction = one_bit_.predict();
        Outcome gshare_prediction = gshare_.predict();
        if (one_bit_prediction != gshare_prediction) {
            if (gshare_prediction == actual) selector_ = std::min(3, selector_ + 1);
            else if (one_bit_prediction == actual) selector_ = std::max(0, selector_ - 1);
        }
        one_bit_.update(actual);
        gshare_.update(actual);
    }

    std::string name() const override { return "Tournament"; }

private:
    OneBitPredictor one_bit_;
    GSharePredictor gshare_;
    int selector_ = 2;
};

double evaluate_tournament(const std::vector<Outcome>& trace) {
    TournamentPredictor predictor;
    return evaluate_predictor(predictor, trace);
}

struct PcOutcome {
    uint64_t pc;
    Outcome outcome;
};

class PcGShare {
public:
    explicit PcGShare(int history_bits) : mask_((1u << history_bits) - 1), table_(1u << history_bits, 2) {}

    Outcome predict(uint64_t pc) const { return table_[(history_ ^ (uint32_t)pc) & mask_] >= 2 ? Outcome::Taken : Outcome::NotTaken; }

    void update(uint64_t pc, Outcome actual) {
        int& counter = table_[(history_ ^ (uint32_t)pc) & mask_];
        if (actual == Outcome::Taken) counter = std::min(3, counter + 1);
        else counter = std::max(0, counter - 1);
        history_ = ((history_ << 1) | (actual == Outcome::Taken ? 1u : 0u)) & mask_;
    }

private:
    uint32_t mask_;
    uint32_t history_ = 0;
    std::vector<int> table_;
};

double evaluate_pc_gshare(const std::vector<PcOutcome>& trace, int history_bits) {
    PcGShare predictor(history_bits);
    int correct = 0;
    for (const PcOutcome& branch : trace) {
        if (predictor.predict(branch.pc) == branch.outcome) ++correct;
        predictor.update(branch.pc, branch.outcome);
    }
    return trace.empty() ? 0.0 : (double)correct / trace.size();
}

void run_pattern(const std::string& label, const std::vector<Outcome>& trace) {
    std::cout << label << " (" << trace.size() << " branches)\n";
    std::cout << "Predictor            Accuracy\n";
    std::cout << "--------------------------------\n";
    for (auto& predictor : make_all_predictors()) {
        double acc = evaluate_predictor(*predictor, trace);
        std::cout << std::left << std::setw(21) << predictor->name() << std::right << std::fixed
                   << std::setprecision(1) << std::setw(5) << (100.0 * acc) << "%\n";
    }
    std::cout << "\n";
}
} // namespace

int main() {
    const int n = 2000;
    std::cout << "Benchmark: predictor accuracy across synthetic branch patterns\n\n";

    run_pattern("Always taken", pattern_always_taken(n));
    run_pattern("Alternating (T N T N ...)", pattern_alternating(n));
    run_pattern("Loop (9 taken, 1 not-taken, repeating)", pattern_loop(n, 10));
    run_pattern("Random (50% taken)", pattern_random(n, 0.5, 42));
    run_pattern("Biased (90% taken)", pattern_random(n, 0.9, 42));

    std::cout << "Experiment 2: GShare history width on a period-10 loop\n\n";
    std::cout << std::left << std::setw(14) << "history bits" << "accuracy\n";
    auto loop = pattern_loop(n, 10);
    for (int bits = 1; bits <= 12; ++bits) {
        GSharePredictor predictor(bits);
        std::cout << std::left << std::setw(14) << bits << std::fixed << std::setprecision(1)
                  << (100.0 * evaluate_predictor(predictor, loop)) << "%\n";
    }

    std::cout << "\nExperiment 3: PC-indexed GShare on two interleaved branches\n\n";
    std::vector<PcOutcome> pc_trace;
    for (int i = 0; i < n; ++i) {
        pc_trace.push_back({0x100, (i % 4 == 0) ? Outcome::NotTaken : Outcome::Taken});
        pc_trace.push_back({0x200, (i % 2 == 0) ? Outcome::Taken : Outcome::NotTaken});
    }
    std::cout << "branches: " << pc_trace.size() << "\n";
    std::cout << "PC-indexed GShare (8-bit history): " << (100.0 * evaluate_pc_gshare(pc_trace, 8)) << "%\n";

    std::cout << "\nExperiment 4: tournament selector\n\n";
    std::cout << std::left << std::setw(18) << "pattern" << std::setw(12) << "1-bit" << std::setw(12)
              << "GShare" << "tournament\n";
    for (const auto& item : {std::pair<const char*, std::vector<Outcome>>{"alternating", pattern_alternating(n)},
                             {"loop", pattern_loop(n, 10)}, {"biased", pattern_random(n, 0.9, 42)}}) {
        OneBitPredictor one_bit;
        GSharePredictor gshare;
        std::cout << std::left << std::setw(18) << item.first << std::fixed << std::setprecision(1)
              << (100.0 * evaluate_predictor(one_bit, item.second)) << "%       "
              << (100.0 * evaluate_predictor(gshare, item.second)) << "%       "
              << (100.0 * evaluate_tournament(item.second)) << "%\n";
    }

    return 0;
}
