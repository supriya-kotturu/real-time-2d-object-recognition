#include "app/frame_mailbox.h"

#include <utility>

std::uint64_t FrameMailbox::put(cv::Mat frame) {
    std::uint64_t seq;
    {
        std::lock_guard lock(m_);
        slot_ = std::move(frame);
        seq = ++putSeq_;
    }
    changed_.notify_all();
    return seq;
}

bool FrameMailbox::take(cv::Mat& frame, std::chrono::milliseconds timeout) {
    {
        std::unique_lock lock(m_);
        if (!changed_.wait_for(lock, timeout, [&] { return closed_ || putSeq_ > takenSeq_; }) ||
            closed_)
            return false;
        frame = slot_;
        slot_.release();  // drop our ref so the pipeline owns the only one
        takenSeq_ = putSeq_;
    }
    changed_.notify_all();
    return true;
}

bool FrameMailbox::waitConsumed(std::uint64_t seq, std::chrono::milliseconds timeout) {
    std::unique_lock lock(m_);
    return changed_.wait_for(lock, timeout,
                             [&] { return closed_ || takenSeq_ >= seq || putSeq_ > seq; }) &&
           !closed_;
}

void FrameMailbox::close() {
    {
        std::lock_guard lock(m_);
        closed_ = true;
    }
    changed_.notify_all();
}
