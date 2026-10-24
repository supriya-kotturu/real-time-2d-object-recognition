#pragma once

#include <opencv2/core.hpp>

namespace objectrecognition {

int isodataThreshold(const cv::Mat& gray);                      // decide t
void applyThreshold(const cv::Mat& gray, int t, cv::Mat& dst);  // use t
}  // namespace objectrecognition