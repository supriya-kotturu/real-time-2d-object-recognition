#include "app/pipeline.h"

#include <opencv2/imgproc.hpp>

#include "core/threshold.h"

namespace orec = objectrecognition;

namespace {
// Scales with resolution so a 1280x720 phone stream and the 640x480 dev set behave alike.
constexpr double kMinAreaFraction = 0.002;
constexpr int kMaxRegions = 5;
}  // namespace

void runPipeline(const cv::Mat& frame, FrameResult& r) {
    cv::cvtColor(frame, r.gray, cv::COLOR_BGR2GRAY);
    r.threshold = orec::isodataThreshold(r.gray);
    orec::applyThreshold(r.gray, r.threshold, r.binary);

    // T2 (grassfire, HAND) slots in here, e.g. closing = grow then shrink. Until then the
    // raw binary goes straight to segmentation.
    r.cleaned = r.binary;

    const int minArea = static_cast<int>(kMinAreaFraction * frame.total());
    orec::segmentRegions(r.cleaned, minArea, kMaxRegions, r.regionMap, r.regions);
    r.primary = orec::primaryRegion(r.regions, frame.size());

    r.hasFeatures = false;
    if (r.primary >= 0) {
        const auto& reg = r.regions[r.primary];
        r.hasFeatures = orec::computeFeatures(r.regionMap, reg.id, reg.bbox, r.features);
    }
}
