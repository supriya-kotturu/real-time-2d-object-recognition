// State-machine tests for the auto-learn extension (D-15), driven by a fake clock.

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

constexpr double kDiag = 1000.0;  // drift limit = 50 px with the default 5%
const cv::Point2d kHere(500, 400);

}  // namespace

int main() {
    const auto t0 = AutoLearn::Clock::time_point{} + 100s;

    {
        AutoLearn al;
        al.update(true, true, kHere, kDiag, t0);
        al.update(true, true, kHere, kDiag, t0 + 2900ms);
        expect(al.state() == State::Tracking, "unknown for 2.9 s keeps tracking");
        al.update(true, true, kHere, kDiag, t0 + 3s);
        expect(al.state() == State::Prompt, "unknown and still for 3 s prompts");
    }
    {
        AutoLearn al;
        al.update(true, true, kHere, kDiag, t0);
        al.update(true, false, kHere, kDiag, t0 + 1s);
        al.update(true, true, kHere, kDiag, t0 + 2s);
        al.update(true, true, kHere, kDiag, t0 + 4s);
        expect(al.state() == State::Tracking, "a known frame restarts the 3 s window");
    }
    {
        AutoLearn al;
        al.update(true, true, kHere, kDiag, t0);
        al.update(true, true, kHere + cv::Point2d(80, 0), kDiag, t0 + 2s);
        al.update(true, true, kHere + cv::Point2d(80, 0), kDiag, t0 + 4s);
        expect(al.state() == State::Tracking, "moving > 5% of the diagonal restarts the window");
    }
    {
        AutoLearn al;
        al.update(true, true, kHere, kDiag, t0);
        al.update(true, true, kHere, kDiag, t0 + 3s);
        al.dismiss();
        al.update(true, true, kHere, kDiag, t0 + 10s);
        expect(al.state() == State::Idle, "dismissed object is not re-prompted while in view");
        al.update(false, false, kHere, kDiag, t0 + 11s);
        al.update(true, true, kHere, kDiag, t0 + 12s);
        al.update(true, true, kHere, kDiag, t0 + 15s);
        expect(al.state() == State::Prompt, "after leaving the frame it can prompt again");
    }
    {
        AutoLearn al;
        al.update(true, true, kHere, kDiag, t0);
        al.update(true, true, kHere, kDiag, t0 + 3s);
        expect(al.startCollecting("mug", t0 + 3s), "label accepted in Prompt");
        expect(al.wantsSample(t0 + 3s), "first sample is due immediately");
        al.sampleTaken(t0 + 3s);
        expect(!al.wantsSample(t0 + 3s + 200ms), "next sample waits for the interval");
        auto t = t0 + 3s;
        for (int i = 1; i < al.targetSamples(); ++i) {
            t += 400ms;
            al.sampleTaken(t);
        }
        expect(al.state() == State::Idle, "returns to Idle after collecting all samples");
    }
    {
        AutoLearn al;
        expect(!al.startCollecting("mug", t0), "label rejected outside Prompt");
    }

    std::cout << "\n" << failures << " failed\n";
    return failures == 0 ? 0 : 1;
}
