#pragma once

#include <chrono>
#include <opencv2/core.hpp>
#include <string>

// Auto-learn extension (D-15, D-17): an object that stays unknown and roughly still for
// holdSeconds triggers a "name this object" prompt. Once labelled, one sample is taken per
// *pose*: after each sample the user must move or rotate the object noticeably, then hold it
// still, before the next one is captured. A burst of near-identical frames would teach the
// DB nothing about pose variation and leave its stdevs near zero.
// Pure state machine: the caller supplies the clock, so tests drive it without sleeping.
class AutoLearn {
   public:
    using Clock = std::chrono::steady_clock;
    enum class State { Idle, Tracking, Prompt, Collecting };

    struct Config {
        double holdSeconds = 3.0;
        double maxDriftFraction = 0.05;  // of the image diagonal; filters out passing hands
        int samples = 5;                 // poses to collect
        double minMoveFraction = 0.10;   // a new pose = moved this far (of the diagonal) ...
        double minRotateDegrees = 20.0;  // ... or rotated this much
        double stillFraction = 0.01;     // "held still" = moving less than this ...
        double stillRotateDegrees = 3.0;
        double stillSeconds = 0.6;  // ... for this long
    };

    AutoLearn() = default;
    explicit AutoLearn(Config config) : config_(config) {}

    // Once per frame. unknown = an object is present and nothing in the DB is within the
    // unknown cutoff (or the DB is too small to classify). theta = primary-axis angle.
    void update(bool hasObject, bool unknown, cv::Point2d centroid, double theta,
                double imageDiagonal, Clock::time_point now);

    bool startCollecting(const std::string& label);  // Prompt only
    void dismiss();  // Prompt: ignore; Collecting: stop early. Both: quiet until it leaves.

    // While Collecting: true when the next pose is ready (first one immediately).
    bool wantsSample(Clock::time_point now) const;
    void sampleTaken(cv::Point2d centroid, double theta);

    State state() const { return state_; }
    bool awaitingMove() const { return state_ == State::Collecting && collected_ > 0 && !moved_; }
    const std::string& label() const { return label_; }
    int collected() const { return collected_; }
    int targetSamples() const { return config_.samples; }
    double holdSeconds() const { return config_.holdSeconds; }
    double holdProgress(Clock::time_point now) const;  // 0..1 while Tracking

   private:
    void reset();
    void updateCollecting(bool hasObject, cv::Point2d centroid, double theta, double diag,
                          Clock::time_point now);

    Config config_;
    State state_ = State::Idle;
    Clock::time_point start_{};
    cv::Point2d anchor_{};
    bool dismissed_ = false;
    std::string label_;
    int collected_ = 0;

    // Collecting: the last captured pose, and the current "held still" window.
    cv::Point2d poseCentroid_{};
    double poseTheta_ = 0.0;
    bool moved_ = false;
    bool stillValid_ = false;
    cv::Point2d stillCentroid_{};
    double stillTheta_ = 0.0;
    Clock::time_point stillStart_{};
};

const char* toString(AutoLearn::State state);
