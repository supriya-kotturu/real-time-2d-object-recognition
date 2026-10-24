#include "app/auto_learn.h"

#include <algorithm>
#include <cmath>

namespace {

double seconds(AutoLearn::Clock::duration d) { return std::chrono::duration<double>(d).count(); }

double distance(cv::Point2d a, cv::Point2d b) { return std::hypot(a.x - b.x, a.y - b.y); }

// The primary axis is undirected, so 10 deg and 190 deg are the same orientation.
double axisDegreesBetween(double a, double b) {
    double d = std::fmod(std::abs(a - b), CV_PI);
    d = std::min(d, CV_PI - d);
    return d * 180.0 / CV_PI;
}

}  // namespace

void AutoLearn::reset() {
    state_ = State::Idle;
    label_.clear();
    collected_ = 0;
    moved_ = false;
    stillValid_ = false;
}

void AutoLearn::update(bool hasObject, bool unknown, cv::Point2d centroid, double theta,
                       double imageDiagonal, Clock::time_point now) {
    if (state_ == State::Collecting) {
        updateCollecting(hasObject, centroid, theta, imageDiagonal, now);
        return;
    }

    if (!hasObject) {
        // The object left the frame, so a dismissed object may be prompted for again later.
        dismissed_ = false;
        reset();
        return;
    }

    const bool drifted = distance(centroid, anchor_) > config_.maxDriftFraction * imageDiagonal;

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

void AutoLearn::updateCollecting(bool hasObject, cv::Point2d centroid, double theta, double diag,
                                 Clock::time_point now) {
    if (!hasObject) {
        stillValid_ = false;  // lifted mid-move: wait for it to come back and settle
        return;
    }
    if (collected_ == 0) return;  // first pose is taken as soon as the label arrives

    if (!moved_) {
        moved_ = distance(centroid, poseCentroid_) >= config_.minMoveFraction * diag ||
                 axisDegreesBetween(theta, poseTheta_) >= config_.minRotateDegrees;
        if (!moved_) return;
    }
    const bool settled = stillValid_ &&
                         distance(centroid, stillCentroid_) <= config_.stillFraction * diag &&
                         axisDegreesBetween(theta, stillTheta_) <= config_.stillRotateDegrees;
    if (!settled) {
        stillValid_ = true;
        stillCentroid_ = centroid;
        stillTheta_ = theta;
        stillStart_ = now;
    }
}

bool AutoLearn::startCollecting(const std::string& label) {
    if (state_ != State::Prompt) return false;
    state_ = State::Collecting;
    label_ = label;
    collected_ = 0;
    moved_ = false;
    stillValid_ = false;
    return true;
}

void AutoLearn::dismiss() {
    // Either way, don't prompt for this object again until it leaves the frame. Stopping a
    // collection early keeps its poses in the DB; with too few to classify, the object would
    // otherwise read as unknown and re-prompt 3 s later.
    if (state_ == State::Prompt || state_ == State::Collecting) {
        dismissed_ = true;
        reset();
    }
}

bool AutoLearn::wantsSample(Clock::time_point now) const {
    if (state_ != State::Collecting) return false;
    if (collected_ == 0) return true;
    return moved_ && stillValid_ && seconds(now - stillStart_) >= config_.stillSeconds;
}

void AutoLearn::sampleTaken(cv::Point2d centroid, double theta) {
    if (state_ != State::Collecting) return;
    poseCentroid_ = centroid;
    poseTheta_ = theta;
    moved_ = false;
    stillValid_ = false;
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
