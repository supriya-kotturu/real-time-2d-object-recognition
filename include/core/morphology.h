#pragma once

#include <opencv2/core.hpp>

namespace objectrecognition {

enum class Connectivity { Four, Eight };

// binary: CV_8UC1, 0 = background, 255 = foreground.
void grassfire(const cv::Mat& frame, cv::Mat& dest, Connectivity conn);
void grow(const cv::Mat& frame, cv::Mat& dest, Connectivity conn, int n);
void shrink(const cv::Mat& frame, cv::Mat& dest, Connectivity conn, int n);
}  // namespace objectrecognition