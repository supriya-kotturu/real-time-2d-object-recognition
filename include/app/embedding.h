#pragma once

#include <opencv2/core.hpp>
#include <opencv2/dnn.hpp>
#include <string>
#include <vector>

#include "app/roi.h"
#include "core/features.h"

// T9 one-shot embeddings (D-16, D-18): ROI extraction lives in roi.h (no dnn dependency).

// Steps D-E: 224x224 ResNet18 input, 512-d output of the global-average-pool/flatten layer.
class Embedder {
   public:
    explicit Embedder(const std::string& onnxPath);  // throws cv::Exception if unloadable
    void embed(const cv::Mat& roiBgr, std::vector<double>& out);

   private:
    cv::dnn::Net net_;
    std::vector<float> input_;  // reused per call
};
