#pragma once

#include <array>
#include <opencv2/core.hpp>
#include <string>
#include <vector>

namespace objectrecognition {

// Order is the feature-DB contract (D-12): changing it means retraining the DB.
inline const std::vector<std::string> kFeatureNames = {
    "percentFilled",  // region area / oriented-bbox area
    "aspectRatio",    // oriented bbox short side / long side, in (0, 1]
    "hu1",            // eta20 + eta02: spread of mass around the centroid
    "hu2",            // (eta20 - eta02)^2 + 4 eta11^2: elongation
    "hu3",            // (eta30 - 3 eta12)^2 + (3 eta21 - eta03)^2: asymmetry
};

struct RegionFeatures {
    cv::Point2d centroid;
    double theta = 0.0;  // axis of least central moment, radians, image coords (y down)
    // Extents of the region along the primary (e1) and secondary (e2) axes, from the centroid.
    double minE1 = 0.0, maxE1 = 0.0, minE2 = 0.0, maxE2 = 0.0;
    std::vector<double> values;  // in kFeatureNames order
};

// T4: region-based (not boundary-based) features, all translation/scale/rotation invariant.
// bbox limits the scan to the region's axis-aligned box. Returns false for an empty region.
bool computeFeatures(const cv::Mat& regionMap, int regionId, const cv::Rect& bbox,
                     RegionFeatures& out);

// Corners of the oriented bounding box (aligned to the primary axis), in image coordinates.
std::array<cv::Point2d, 4> orientedBox(const RegionFeatures& f);

}  // namespace objectrecognition
