#include "app/embedding.h"

#include <cmath>
#include <opencv2/geometry.hpp>  // OpenCV 5: getRotationMatrix2D moved here (H-41)
#include <opencv2/imgproc.hpp>

namespace {

// ResNet18 (ONNX zoo v2) penultimate layer: 512 values after global average pooling.
const char* kEmbeddingLayer = "onnx_node!resnetv22_flatten0_reshape0";

// Sign of the third central moment along the axis: which end carries the long tail.
double axisSkew(const cv::Mat& regionMap, int regionId,
                const objectrecognition::RegionFeatures& f) {
    const double ct = std::cos(f.theta), st = std::sin(f.theta);
    double m3 = 0;
    for (int r = 0; r < regionMap.rows; ++r) {
        const int* row = regionMap.ptr<int>(r);
        for (int c = 0; c < regionMap.cols; ++c) {
            if (row[c] != regionId) continue;
            const double e1 = (c - f.centroid.x) * ct + (r - f.centroid.y) * st;
            m3 += e1 * e1 * e1;
        }
    }
    return m3;
}

}  // namespace

bool extractAlignedRoi(const cv::Mat& frame, const cv::Mat& regionMap, int regionId,
                       const objectrecognition::RegionFeatures& f, cv::Mat& roi) {
    // Flip the axis by 180 deg when needed: rotating by pi negates both extents.
    double theta = f.theta;
    double minE1 = f.minE1, maxE1 = f.maxE1, minE2 = f.minE2, maxE2 = f.maxE2;
    if (axisSkew(regionMap, regionId, f) < 0) {
        theta += CV_PI;
        minE1 = -f.maxE1;
        maxE1 = -f.minE1;
        minE2 = -f.maxE2;
        maxE2 = -f.minE2;
    }

    // Our theta is measured in image coordinates (y down), so rotating by +theta maps the
    // axis u = (cos, sin) onto +x and v = (-sin, cos) onto +y. The canvas is the frame's
    // diagonal, centred on the centroid, so no part of the object is clipped by the rotation.
    const int side = static_cast<int>(std::ceil(std::hypot(frame.cols, frame.rows)));
    cv::Mat m = cv::getRotationMatrix2D(f.centroid, theta * 180.0 / CV_PI, 1.0);
    m.at<double>(0, 2) += side / 2.0 - f.centroid.x;
    m.at<double>(1, 2) += side / 2.0 - f.centroid.y;
    cv::Mat rotated;
    cv::warpAffine(frame, rotated, m, {side, side}, cv::INTER_LINEAR, cv::BORDER_REPLICATE);

    // +1: extents run between pixel centres; the box must cover the edge pixels fully.
    const cv::Rect box(static_cast<int>(std::floor(side / 2.0 + minE1)),
                       static_cast<int>(std::floor(side / 2.0 + minE2)),
                       static_cast<int>(std::ceil(maxE1 - minE1)) + 1,
                       static_cast<int>(std::ceil(maxE2 - minE2)) + 1);
    const cv::Rect clipped = box & cv::Rect(0, 0, side, side);
    if (clipped.empty()) return false;
    rotated(clipped).copyTo(roi);
    return true;
}

Embedder::Embedder(const std::string& onnxPath) : net_(cv::dnn::readNetFromONNX(onnxPath)) {}

void Embedder::embed(const cv::Mat& roiBgr, std::vector<double>& out) {
    // ImageNet normalisation as in the course code: mean (124,116,104) RGB, stdev ~0.226.
    // blobFromImage resizes to 224x224 itself (no crop), so elongated objects get squashed:
    // the same squash happens for DB and query images, so it is consistent.
    const cv::Mat blob = cv::dnn::blobFromImage(roiBgr, 1.0 / (255.0 * 0.226), {224, 224},
                                                cv::Scalar(124, 116, 104), true, false, CV_32F);
    net_.setInput(blob);
    const cv::Mat e = net_.forward(kEmbeddingLayer);
    out.assign(e.ptr<float>(), e.ptr<float>() + e.total());
}
