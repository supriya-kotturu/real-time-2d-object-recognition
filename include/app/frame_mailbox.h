#pragma once

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <opencv2/core.hpp>

// Single-slot handoff from a push source (the browser camera over WebSocket, D-24) to the
// pipeline thread. A new frame replaces an unread one: under load stale frames are dropped
// instead of queued, so latency stays at one frame. waitConsumed() lets the sender hold
// exactly one frame in flight (H-50): it sends the next frame only after this one was taken.
class FrameMailbox {
   public:
    // Returns the frame's sequence number, for waitConsumed().
    std::uint64_t put(cv::Mat frame);

    // Waits up to `timeout` for a frame newer than the last one taken.
    bool take(cv::Mat& frame, std::chrono::milliseconds timeout);

    // True once frame `seq` was taken, or replaced by a newer one before it could be: either
    // way the sender may send again. False on timeout or close().
    bool waitConsumed(std::uint64_t seq, std::chrono::milliseconds timeout);

    // Wakes every waiter; take() and waitConsumed() return false from then on.
    void close();

   private:
    std::mutex m_;
    std::condition_variable changed_;
    cv::Mat slot_;
    std::uint64_t putSeq_ = 0;
    std::uint64_t takenSeq_ = 0;
    bool closed_ = false;
};
