#pragma once

#include <opencv2/core.hpp>
#include <vector>

#include "core/features.h"
#include "core/regions.h"

// One frame through tasks 1-4. Shared by live and inspect so both show identical results.
// Members persist across frames so each stage's out-param reuses its buffer.
struct FrameResult {
    int threshold = 0;
    cv::Mat gray, binary, cleaned, regionMap;
    std::vector<objectrecognition::Region> regions;
    int primary = -1;  // index into regions, -1 = no usable region
    objectrecognition::RegionFeatures features;
    bool hasFeatures = false;
};

void runPipeline(const cv::Mat& frame, FrameResult& r);
