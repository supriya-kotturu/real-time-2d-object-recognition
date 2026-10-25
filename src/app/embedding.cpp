#include "app/embedding.h"

namespace {
// ResNet18 (ONNX zoo v2) penultimate layer: 512 values after global average pooling.
const char* kEmbeddingLayer = "onnx_node!resnetv22_flatten0_reshape0";
}  // namespace

Embedder::Embedder(const std::string& onnxPath) : net_(cv::dnn::readNetFromONNX(onnxPath)) {}

void Embedder::embed(const cv::Mat& roiBgr, std::vector<double>& out) {
    // The browser build prepares its input with the same function, so both produce the same
    // embedding for the same ROI.
    makeResnetInput(roiBgr, input_);
    const int shape[] = {1, 3, kResnetSide, kResnetSide};
    net_.setInput(cv::Mat(4, shape, CV_32F, input_.data()));
    const cv::Mat e = net_.forward(kEmbeddingLayer);
    out.assign(e.ptr<float>(), e.ptr<float>() + e.total());
}
