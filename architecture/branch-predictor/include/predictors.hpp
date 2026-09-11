#pragma once

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

enum class Outcome { NotTaken, Taken };

class Predictor {
public:
    virtual ~Predictor() = default;
    virtual Outcome predict() const = 0; // guess for the NEXT branch, before seeing it
    virtual void update(Outcome actual) = 0; // called after the real outcome is known
    virtual std::string name() const = 0;
};

class AlwaysTaken : public Predictor {
public:
    Outcome predict() const override { return Outcome::Taken; }
    void update(Outcome) override {}
    std::string name() const override { return "Always Taken"; }
};

class AlwaysNotTaken : public Predictor {
public:
    Outcome predict() const override { return Outcome::NotTaken; }
    void update(Outcome) override {}
    std::string name() const override { return "Always Not Taken"; }
};

// Predicts whatever happened last time.
class OneBitPredictor : public Predictor {
public:
    Outcome predict() const override { return last_; }
    void update(Outcome actual) override { last_ = actual; }
    std::string name() const override { return "1-bit"; }

private:
    Outcome last_ = Outcome::Taken;
};

// 2-bit saturating counter (0..3, >=2 predicts Taken). Needs two
// consecutive outcomes in the same direction to flip its prediction,
// so a single "glitch" against an otherwise consistent trend (e.g. a
// loop's final not-taken exit) costs one misprediction here instead of
// the two it costs the 1-bit predictor.
class TwoBitPredictor : public Predictor {
public:
    Outcome predict() const override { return counter_ >= 2 ? Outcome::Taken : Outcome::NotTaken; }
    void update(Outcome actual) override {
        if (actual == Outcome::Taken) counter_ = std::min(3, counter_ + 1);
        else counter_ = std::max(0, counter_ - 1);
    }
    std::string name() const override { return "2-bit"; }

private:
    int counter_ = 2; // start "weakly taken"
};

// GShare: an N-bit global history register indexes a table of 2-bit
// saturating counters, so the prediction depends on the recent PATTERN
// of outcomes, not just the single most recent one or a fixed guess.
// A real multi-branch gshare indexes with `history XOR PC`; this lab's
// trace format has one implicit branch stream, so the index is the
// history register alone (equivalent to XOR-ing with a constant PC=0
// — see the README).
class GSharePredictor : public Predictor {
public:
    explicit GSharePredictor(int history_bits = 8)
        : mask_((1u << history_bits) - 1), table_(1u << history_bits, 2) {}

    Outcome predict() const override { return table_[history_] >= 2 ? Outcome::Taken : Outcome::NotTaken; }

    void update(Outcome actual) override {
        int& counter = table_[history_];
        if (actual == Outcome::Taken) counter = std::min(3, counter + 1);
        else counter = std::max(0, counter - 1);
        history_ = ((history_ << 1) | (actual == Outcome::Taken ? 1u : 0u)) & mask_;
    }

    std::string name() const override { return "GShare"; }

private:
    uint32_t mask_;
    uint32_t history_ = 0;
    std::vector<int> table_;
};

inline std::vector<std::unique_ptr<Predictor>> make_all_predictors() {
    std::vector<std::unique_ptr<Predictor>> v;
    v.push_back(std::make_unique<AlwaysTaken>());
    v.push_back(std::make_unique<AlwaysNotTaken>());
    v.push_back(std::make_unique<OneBitPredictor>());
    v.push_back(std::make_unique<TwoBitPredictor>());
    v.push_back(std::make_unique<GSharePredictor>());
    return v;
}

// Runs `predictor` over `trace`, predicting each outcome before
// revealing it, and returns the fraction correctly predicted.
inline double evaluate_predictor(Predictor& predictor, const std::vector<Outcome>& trace) {
    if (trace.empty()) return 0.0;
    long long correct = 0;
    for (Outcome actual : trace) {
        if (predictor.predict() == actual) correct++;
        predictor.update(actual);
    }
    return (double)correct / (double)trace.size();
}
