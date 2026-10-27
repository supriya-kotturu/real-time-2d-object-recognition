// Tests for the browser-camera handoff (D-24): latest frame wins, and the one-in-flight ack.

#include <chrono>
#include <iostream>
#include <string>
#include <thread>

#include "app/frame_mailbox.h"

using namespace std::chrono_literals;

namespace {

int failures = 0;

void expect(bool ok, const std::string& what) {
    std::cout << (ok ? "[ OK ] " : "[FAIL] ") << what << "\n";
    if (!ok) ++failures;
}

cv::Mat frameOf(uchar value) { return cv::Mat(2, 2, CV_8UC1, cv::Scalar(value)); }

}  // namespace

int main() {
    {
        FrameMailbox box;
        cv::Mat out;
        expect(!box.take(out, 10ms), "take on an empty mailbox times out");
    }
    {
        FrameMailbox box;
        box.put(frameOf(1));
        box.put(frameOf(2));
        cv::Mat out;
        expect(box.take(out, 10ms) && out.at<uchar>(0, 0) == 2, "a newer frame replaces an unread one");
        expect(!box.take(out, 10ms), "a frame is taken only once");
    }
    {
        FrameMailbox box;
        const auto seq = box.put(frameOf(1));
        expect(!box.waitConsumed(seq, 10ms), "an untaken frame is not acked");
        cv::Mat out;
        box.take(out, 10ms);
        expect(box.waitConsumed(seq, 10ms), "a taken frame is acked");
    }
    {
        // Two tabs: the first sender's frame is overwritten before the pipeline reads it.
        FrameMailbox box;
        const auto first = box.put(frameOf(1));
        box.put(frameOf(2));
        expect(box.waitConsumed(first, 10ms), "a replaced frame releases its sender");
    }
    {
        FrameMailbox box;
        const auto seq = box.put(frameOf(1));
        std::thread consumer([&] {
            std::this_thread::sleep_for(20ms);
            cv::Mat out;
            box.take(out, 100ms);
        });
        expect(box.waitConsumed(seq, 1s), "waitConsumed wakes when another thread takes the frame");
        consumer.join();
    }
    {
        FrameMailbox box;
        const auto seq = box.put(frameOf(1));
        std::thread closer([&] {
            std::this_thread::sleep_for(20ms);
            box.close();
        });
        const auto start = std::chrono::steady_clock::now();
        const bool acked = box.waitConsumed(seq, 5s);
        closer.join();
        expect(!acked && std::chrono::steady_clock::now() - start < 1s,
               "close() wakes a blocked sender (server shutdown)");
        cv::Mat out;
        expect(!box.take(out, 10ms), "take after close() returns false");
    }

    std::cout << (failures ? "FAILED: " : "all passed, failures: ") << failures << "\n";
    return failures ? 1 : 0;
}
