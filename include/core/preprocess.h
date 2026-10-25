#pragma once

#include <opencv2/core.hpp>

namespace objectrecognition {

// Task 1 pre-processing (D-22): BGR frame -> single-channel image for thresholding in which
// strongly coloured pixels are darkened: out = V * (1 - strength * S). White paper and grey
// shadows have near-zero saturation and keep their brightness, so a pink or orange object is
// pushed away from the background while shadows are not. strength in [0, 1].
void saturationDarken(const cv::Mat& bgr, double strength, cv::Mat& dst);

}  // namespace objectrecognition
