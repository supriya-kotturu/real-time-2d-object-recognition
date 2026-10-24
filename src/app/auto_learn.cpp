#include "app/auto_learn.h"

#include <algorithm>
#include <cmath>

namespace {

double seconds(AutoLearn::Clock::duration d) { return std::chrono::duration<double>(d).count(); }

}  // namespace

void AutoLearn::reset() {
    state_ = State::Idle;
    label_.clear();
    collected_ = 0;
}

void AutoLearn::update(bool hasObject, bool unknown, cv::Point2d centroid, double imageDiagonal,
                       Clock::time_point now) {
    if (state_ == State::Collecting) return;  // sampling is driven by wantsSample/sampleTaken

    if (!hasObject) {
        // The object left the frame, so a dismissed object may be prompted for again later.
        dismissed_ = false;
        reset();
        return;
    }

    const cv::Point2d d = centroid - anchor_;
    const bool drifted = std::hypot(d.x, d.y) > config_.maxDriftFraction * imageDiagonal;

    if (!unknown || dismissed_) {
        reset();
        return;
    }
    if (state_ == State::Prompt) {
        if (drifted) reset();  // a different object, or the user moved it away
        return;
    }
    if (state_ == State::Idle || drifted) {
        state_ = State::Tracking;
        start_ = now;
        anchor_ = centroid;
        return;
    }
    if (seconds(now - start_) >= config_.holdSeconds) state_ = State::Prompt;
}

bool AutoLearn::startCollecting(const std::string& label, Clock::time_point now) {
    if (state_ != State::Prompt) return false;
    state_ = State::Collecting;
    label_ = label;
    collected_ = 0;
    // Due immediately: the first sample is the frame the user was looking at.
    lastSample_ = now - std::chrono::duration_cast<Clock::duration>(
                            std::chrono::duration<double>(config_.sampleIntervalSeconds));
    return true;
}

void AutoLearn::dismiss() {
    if (state_ != State::Prompt) return;
    dismissed_ = true;
    reset();
}

bool AutoLearn::wantsSample(Clock::time_point now) const {
    return state_ == State::Collecting &&
           seconds(now - lastSample_) >= config_.sampleIntervalSeconds;
}

void AutoLearn::sampleTaken(Clock::time_point now) {
    if (state_ != State::Collecting) return;
    lastSample_ = now;
    if (++collected_ >= config_.samples) {
        reset();  // now in the DB, so it classifies as known and won't re-trigger
    }
}

double AutoLearn::holdProgress(Clock::time_point now) const {
    if (state_ == State::Prompt) return 1.0;
    if (state_ != State::Tracking) return 0.0;
    return std::clamp(seconds(now - start_) / config_.holdSeconds, 0.0, 1.0);
}

const char* toString(AutoLearn::State state) {
    switch (state) {
        case AutoLearn::State::Idle: return "idle";
        case AutoLearn::State::Tracking: return "tracking";
        case AutoLearn::State::Prompt: return "prompt";
        case AutoLearn::State::Collecting: return "collecting";
    }
    return "idle";
}
