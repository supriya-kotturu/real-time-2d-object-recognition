// State-machine tests for the auto-learn extension (D-15, D-17), driven by a fake clock.

#include <chrono>
#include <iostream>
#include <string>

#include "app/auto_learn.h"

using namespace std::chrono_literals;
using State = AutoLearn::State;

namespace {

int failures = 0;

void expect(bool ok, const std::string& what) {
    std::cout << (ok ? "[ OK ] " : "[FAIL] ") << what << "\n";
    if (!ok) ++failures;
}

constexpr double kDiag = 1000.0;  // drift 50 px, move 100 px, still 10 px with the defaults
const cv::Point2d kHere(500, 400);
const auto t0 = AutoLearn::Clock::time_point{} + 100s;

// Drives an object from Idle to Prompt at kHere.
void prompt(AutoLearn& al) {
    al.update(true, true, kHere, 0.0, kDiag, t0);
    al.update(true, true, kHere, 0.0, kDiag, t0 + 3s);
}

}  // namespace

int main() {
    {
        AutoLearn al;
        al.update(true, true, kHere, 0.0, kDiag, t0);
        al.update(true, true, kHere, 0.0, kDiag, t0 + 2900ms);
        expect(al.state() == State::Tracking, "unknown for 2.9 s keeps tracking");
        al.update(true, true, kHere, 0.0, kDiag, t0 + 3s);
        expect(al.state() == State::Prompt, "unknown and still for 3 s prompts");
    }
    {
        AutoLearn al;
        al.update(true, true, kHere, 0.0, kDiag, t0);
        al.update(true, false, kHere, 0.0, kDiag, t0 + 1s);
        al.update(true, true, kHere, 0.0, kDiag, t0 + 2s);
        al.update(true, true, kHere, 0.0, kDiag, t0 + 4s);
        expect(al.state() == State::Tracking, "a known frame restarts the 3 s window");
    }
    {
        AutoLearn al;
        al.update(true, true, kHere, 0.0, kDiag, t0);
        al.update(true, true, kHere + cv::Point2d(80, 0), 0.0, kDiag, t0 + 2s);
        al.update(true, true, kHere + cv::Point2d(80, 0), 0.0, kDiag, t0 + 4s);
        expect(al.state() == State::Tracking, "moving > 5% of the diagonal restarts the window");
    }
    {
        AutoLearn al;
        prompt(al);
        al.dismiss();
        al.update(true, true, kHere, 0.0, kDiag, t0 + 10s);
        expect(al.state() == State::Idle, "dismissed object is not re-prompted while in view");
        al.update(false, false, kHere, 0.0, kDiag, t0 + 11s);
        al.update(true, true, kHere, 0.0, kDiag, t0 + 12s);
        al.update(true, true, kHere, 0.0, kDiag, t0 + 15s);
        expect(al.state() == State::Prompt, "after leaving the frame it can prompt again");
    }
    {
        AutoLearn al;
        expect(!al.startCollecting("mug"), "label rejected outside Prompt");
    }

    // --- Per-pose collection (D-17) ---
    {
        AutoLearn al;
        prompt(al);
        expect(al.startCollecting("mug"), "label accepted in Prompt");
        auto t = t0 + 3s;
        expect(al.wantsSample(t), "first pose is captured immediately");
        al.sampleTaken(kHere, 0.0);
        expect(al.awaitingMove(), "then waits for the object to be moved");

        // Holding the same pose for a long time must not produce another sample.
        for (int i = 0; i < 20; ++i) al.update(true, true, kHere, 0.0, kDiag, t += 200ms);
        expect(!al.wantsSample(t), "an unmoved object never yields a second pose");

        // Small jitter (< 10% / 20 deg) still isn't a new pose.
        al.update(true, true, kHere + cv::Point2d(40, 0), 0.15, kDiag, t += 200ms);
        al.update(true, true, kHere + cv::Point2d(40, 0), 0.15, kDiag, t += 1s);
        expect(!al.wantsSample(t), "jitter below the move/rotate thresholds is ignored");

        // Rotate 30 deg in place: a new pose, captured once held still for 0.6 s.
        const double rotated = 30.0 * CV_PI / 180.0;
        al.update(true, true, kHere, rotated, kDiag, t += 200ms);
        expect(!al.awaitingMove(), "a 30 deg rotation counts as a new pose");
        al.update(true, true, kHere, rotated, kDiag, t += 300ms);
        expect(!al.wantsSample(t), "not captured before it has been still for 0.6 s");
        al.update(true, true, kHere, rotated, kDiag, t += 400ms);
        expect(al.wantsSample(t), "captured after 0.6 s still");
        al.sampleTaken(kHere, rotated);

        // Rotating by ~180 deg is the same undirected axis: not a new pose.
        al.update(true, true, kHere, rotated + CV_PI, kDiag, t += 200ms);
        al.update(true, true, kHere, rotated + CV_PI, kDiag, t += 1s);
        expect(!al.wantsSample(t), "a 180 deg turn is the same axis, not a new pose");

        // Translate 150 px for the remaining poses.
        for (int pose = 3; pose <= al.targetSamples(); ++pose) {
            const cv::Point2d at = kHere + cv::Point2d(150.0 * pose, 0);
            al.update(true, true, at, rotated, kDiag, t += 200ms);
            al.update(true, true, at, rotated, kDiag, t += 700ms);
            if (al.wantsSample(t)) al.sampleTaken(at, rotated);
        }
        expect(al.state() == State::Idle, "returns to Idle after all 5 poses");
    }
    {
        AutoLearn al;
        prompt(al);
        al.startCollecting("mug");
        al.sampleTaken(kHere, 0.0);
        al.dismiss();
        expect(al.state() == State::Idle, "dismiss during collection stops early");
        al.update(true, true, kHere, 0.0, kDiag, t0 + 5s);
        al.update(true, true, kHere, 0.0, kDiag, t0 + 9s);
        expect(al.state() == State::Idle, "a stopped object is not re-prompted while in view");
    }

    std::cout << "\n" << failures << " failed\n";
    return failures == 0 ? 0 : 1;
}
