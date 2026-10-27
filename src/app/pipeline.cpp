#include "app/pipeline.h"

#include <opencv2/imgproc.hpp>

#include "core/morphology.h"
#include "core/preprocess.h"
#include "core/threshold.h"

namespace orec = objectrecognition;

namespace {
// Scales with resolution so a 1280x720 phone stream and the 640x480 dev set behave alike.
constexpr double kMinAreaFraction = 0.002;
constexpr int kMaxRegions = 5;
constexpr orec::Connectivity kConn = orec::Connectivity::Eight;
constexpr int kCloseN = 2;
constexpr int kOpenN = 1;
// Full strength: a fully saturated pixel goes to black (D-22).
constexpr double kSaturationStrength = 1.0;
}  // namespace

void runPipeline(const cv::Mat& frame, FrameResult& r) {
    // Task 1 pre-processing: coloured objects (pink towel, orange ball) sit close to the paper
    // in plain grayscale; darkening by saturation separates them without darkening shadows.
    orec::saturationDarken(frame, kSaturationStrength, r.gray);
    r.threshold = orec::isodataThreshold(r.gray);
    orec::applyThreshold(r.gray, r.threshold, r.binary);

    // T2: closing first (grow then shrink) seals glare gaps and thin breaks inside the object
    // before anything shrinks them wider; the opening after it trims specks and spurs the
    // closing may have bridged. Proposed n values, to be tuned on real captures (D-21).
    orec::grow(r.binary, r.scratch, kConn, kCloseN);
    orec::shrink(r.scratch, r.cleaned, kConn, kCloseN);
    orec::shrink(r.cleaned, r.scratch, kConn, kOpenN);
    orec::grow(r.scratch, r.cleaned, kConn, kOpenN);

    const int minArea = static_cast<int>(kMinAreaFraction * frame.total());
    orec::segmentRegions(r.cleaned, minArea, kMaxRegions, r.regionMap, r.regions);
    r.primary = orec::primaryRegion(r.regions, frame.size());

    r.hasFeatures = false;
    if (r.primary >= 0) {
        const auto& reg = r.regions[r.primary];
        r.hasFeatures = orec::computeFeatures(r.regionMap, reg.id, reg.bbox, r.features);
    }
}
