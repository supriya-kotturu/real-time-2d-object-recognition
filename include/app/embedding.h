#pragma once

#include <opencv2/core.hpp>
#include <opencv2/dnn.hpp>
#include <string>
#include <vector>

#include "core/features.h"

// T9 one-shot embeddings (D-16, D-18). Approach follows Bruce A. Maxwell's course utilities
// (third_party/maxwell/utilities.cpp); rewritten for our RegionFeatures and OpenCV 5.

// Steps A-C: rotate the ORIGINAL frame about the centroid so the primary axis points along +x,
// then crop the oriented bounding box. The axis has a 180 deg ambiguity; it is resolved so the
// region's third moment along the axis is positive, so the same object rotated by 180 deg
// yields the same ROI. Returns false if nothing of the box lies inside the rotated frame.
bool extractAlignedRoi(const cv::Mat& frame, const cv::Mat& regionMap, int regionId,
                       const objectrecognition::RegionFeatures& f, cv::Mat& roi);

// Steps D-E: 224x224 ResNet18 input, 512-d output of the global-average-pool/flatten layer.
class Embedder {
   public:
    explicit Embedder(const std::string& onnxPath);  // throws cv::Exception if unloadable
    void embed(const cv::Mat& roiBgr, std::vector<double>& out);

   private:
    cv::dnn::Net net_;
};
