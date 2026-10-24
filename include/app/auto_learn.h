#pragma once

#include <chrono>
#include <opencv2/core.hpp>
#include <string>

// Auto-learn extension (D-15): an object that stays unknown and roughly still for holdSeconds
// triggers a "name this object" prompt; once labelled, a few samples are collected over time
// so the DB gets its spread, not just one snapshot. Pure state machine: the caller supplies
// the clock, so tests can drive it without sleeping.
class AutoLearn {
   public:
    using Clock = std::chrono::steady_clock;
    enum class State { Idle, Tracking, Prompt, Collecting };

    struct Config {
        double holdSeconds = 3.0;
        double maxDriftFraction = 0.05;  // of the image diagonal; filters out passing hands
        int samples = 5;
        double sampleIntervalSeconds = 0.4;
    };

    AutoLearn() = default;
    explicit AutoLearn(Config config) : config_(config) {}

    // Once per frame. unknown = an object is present and nothing in the DB is within the
    // unknown cutoff (or the DB is too small to classify at all).
    void update(bool hasObject, bool unknown, cv::Point2d centroid, double imageDiagonal,
                Clock::time_point now);

    bool startCollecting(const std::string& label, Clock::time_point now);  // Prompt only
    void dismiss();  // Prompt only; suppressed until the object leaves the frame

    // While Collecting: true when the next sample is due.
    bool wantsSample(Clock::time_point now) const;
    void sampleTaken(Clock::time_point now);

    State state() const { return state_; }
    const std::string& label() const { return label_; }
    int collected() const { return collected_; }
    int targetSamples() const { return config_.samples; }
    double holdSeconds() const { return config_.holdSeconds; }
    double holdProgress(Clock::time_point now) const;  // 0..1 while Tracking

   private:
    void reset();

    Config config_;
    State state_ = State::Idle;
    Clock::time_point start_{};
    Clock::time_point lastSample_{};
    cv::Point2d anchor_{};
    bool dismissed_ = false;
    std::string label_;
    int collected_ = 0;
};

const char* toString(AutoLearn::State state);
