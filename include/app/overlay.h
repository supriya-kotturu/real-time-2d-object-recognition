#pragma once

#include <opencv2/core.hpp>
#include <string>

#include "core/features.h"

// Region map (CV_32S) -> BGR image, one fixed palette colour per region id (Task 3 display).
void colorizeRegions(const cv::Mat& regionMap, cv::Mat& dst);

// Oriented bbox, primary axis, centroid, and a text label at the region (Task 4/6 display).
void drawFeatures(cv::Mat& img, const objectrecognition::RegionFeatures& f,
                  const std::string& text);
