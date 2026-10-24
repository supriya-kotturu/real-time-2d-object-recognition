#include "core/morphology.h"

#include <algorithm>

// The shiny steel reflects light, so it came out bright and was classified as paper. IF only
// both shrinking and growing are important here.
// first we do closing, then opening
namespace objectrecognition {

namespace {

// Two-pass grassfire (distance to the nearest background pixel). outside = the value a
// neighbour beyond the image edge contributes: 0 makes the border count as background (D-08),
// a large value makes it count as foreground.
void distanceTransform(const cv::Mat& frame, cv::Mat& dest, Connectivity conn, int outside) {
    CV_Assert(frame.type() == CV_8UC1);
    dest.create(frame.size(), CV_32S);
    const bool eight = conn == Connectivity::Eight;
    const int rows = frame.rows, cols = frame.cols;

    // Forward pass (left->right, top->bottom), fg only: min(UP, LEFT [, UP_LEFT, UP_RIGHT]) + 1.
    // Those neighbours are already final for this pass, so bg/fg init happens in the same loop.
    for (int r = 0; r < rows; ++r) {
        const uchar* in = frame.ptr<uchar>(r);
        int* out = dest.ptr<int>(r);
        const int* above = r > 0 ? dest.ptr<int>(r - 1) : nullptr;
        for (int c = 0; c < cols; ++c) {
            if (!in[c]) {
                out[c] = 0;
                continue;
            }
            int m = std::min(above ? above[c] : outside, c > 0 ? out[c - 1] : outside);
            if (eight) {
                m = std::min(m, above && c > 0 ? above[c - 1] : outside);
                m = std::min(m, above && c + 1 < cols ? above[c + 1] : outside);
            }
            out[c] = m + 1;
        }
    }

    // Backward pass (right->left, bottom->top):
    // min(curr, min(DOWN, RIGHT [, DOWN_LEFT, DOWN_RIGHT]) + 1).
    for (int r = rows - 1; r >= 0; --r) {
        const uchar* in = frame.ptr<uchar>(r);
        int* out = dest.ptr<int>(r);
        const int* below = r + 1 < rows ? dest.ptr<int>(r + 1) : nullptr;
        for (int c = cols - 1; c >= 0; --c) {
            if (!in[c]) continue;
            int m = std::min(below ? below[c] : outside, c + 1 < cols ? out[c + 1] : outside);
            if (eight) {
                m = std::min(m, below && c > 0 ? below[c - 1] : outside);
                m = std::min(m, below && c + 1 < cols ? below[c + 1] : outside);
            }
            out[c] = std::min(out[c], m + 1);
        }
    }
}

}  // namespace

void grassfire(const cv::Mat& frame, cv::Mat& dest, Connectivity conn) {
    // if the neighbour falls outside the image, consider it as a bg (D-08)
    distanceTransform(frame, dest, conn, 0);
}

void grow(const cv::Mat& frame, cv::Mat& dest, Connectivity conn, int n) {
    // get the distance from the inverted image: each bg pixel's distance to the nearest fg.
    cv::Mat inverted, dist;
    cv::bitwise_not(frame, inverted);
    // The border must NOT count as background here: in the inverted image that would mean
    // "the border is original foreground", and every bg pixel within n of the image edge
    // would turn fg. So outside-the-image contributes "far away" instead of 0.
    distanceTransform(inverted, dist, conn, frame.rows + frame.cols);

    // original fg has distance 0; bg within n of fg becomes fg
    dest.create(frame.size(), CV_8UC1);
    for (int r = 0; r < frame.rows; ++r) {
        const int* d = dist.ptr<int>(r);
        uchar* out = dest.ptr<uchar>(r);
        for (int c = 0; c < frame.cols; ++c) out[c] = d[c] <= n ? 255 : 0;
    }
}

void shrink(const cv::Mat& frame, cv::Mat& dest, Connectivity conn, int n) {
    cv::Mat dist;
    // Outside the image counts as far, not as background (D-08 revisited): with border = bg,
    // shrinking peeled every object off the image edge, so T3's "touches the border" filter
    // never fired and objects cut off by the frame were classified. Matches cv::erode.
    distanceTransform(frame, dist, conn, frame.rows + frame.cols);

    // if the value is <= n, mark it as bg
    dest.create(frame.size(), CV_8UC1);
    for (int r = 0; r < frame.rows; ++r) {
        const int* d = dist.ptr<int>(r);
        uchar* out = dest.ptr<uchar>(r);
        for (int c = 0; c < frame.cols; ++c) out[c] = d[c] <= n ? 0 : 255;
    }
}
}  // namespace objectrecognition
