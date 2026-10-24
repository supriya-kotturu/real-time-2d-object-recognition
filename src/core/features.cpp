#include "core/features.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace objectrecognition {

bool computeFeatures(const cv::Mat& regionMap, int regionId, const cv::Rect& bbox,
                     RegionFeatures& out) {
    CV_Assert(regionMap.type() == CV_32SC1);
    const cv::Rect box = bbox & cv::Rect(0, 0, regionMap.cols, regionMap.rows);

    // Pass 1: area and centroid. Central moments need the centroid first.
    double m00 = 0, m10 = 0, m01 = 0;
    for (int r = box.y; r < box.y + box.height; ++r) {
        const int* row = regionMap.ptr<int>(r);
        for (int c = box.x; c < box.x + box.width; ++c) {
            if (row[c] != regionId) continue;
            m00 += 1;
            m10 += c;
            m01 += r;
        }
    }
    if (m00 == 0) return false;
    const double cx = m10 / m00, cy = m01 / m00;

    // Pass 2: central moments up to third order.
    double mu20 = 0, mu02 = 0, mu11 = 0, mu30 = 0, mu03 = 0, mu21 = 0, mu12 = 0;
    for (int r = box.y; r < box.y + box.height; ++r) {
        const int* row = regionMap.ptr<int>(r);
        const double dy = r - cy;
        for (int c = box.x; c < box.x + box.width; ++c) {
            if (row[c] != regionId) continue;
            const double dx = c - cx;
            mu20 += dx * dx;
            mu02 += dy * dy;
            mu11 += dx * dy;
            mu30 += dx * dx * dx;
            mu03 += dy * dy * dy;
            mu21 += dx * dx * dy;
            mu12 += dx * dy * dy;
        }
    }
    // Axis of least central moment: the direction the mass is most spread along.
    const double theta = 0.5 * std::atan2(2.0 * mu11, mu20 - mu02);

    // Pass 3: extents along the primary/secondary axes define the oriented bbox. This is
    // deliberately not cv::minAreaRect, which needn't align with theta (task 4 note).
    const double ct = std::cos(theta), st = std::sin(theta);
    double minE1 = std::numeric_limits<double>::max(), maxE1 = -minE1;
    double minE2 = minE1, maxE2 = -minE1;
    for (int r = box.y; r < box.y + box.height; ++r) {
        const int* row = regionMap.ptr<int>(r);
        const double dy = r - cy;
        for (int c = box.x; c < box.x + box.width; ++c) {
            if (row[c] != regionId) continue;
            const double dx = c - cx;
            const double e1 = dx * ct + dy * st;
            const double e2 = -dx * st + dy * ct;
            minE1 = std::min(minE1, e1);
            maxE1 = std::max(maxE1, e1);
            minE2 = std::min(minE2, e2);
            maxE2 = std::max(maxE2, e2);
        }
    }

    // +1: extents run between pixel centres, but each pixel covers a full unit square.
    const double len1 = maxE1 - minE1 + 1.0, len2 = maxE2 - minE2 + 1.0;
    const double percentFilled = m00 / (len1 * len2);
    const double aspectRatio = std::min(len1, len2) / std::max(len1, len2);

    // Normalised central moments: dividing by m00^(1 + (p+q)/2) removes scale.
    const auto eta = [m00](double mu, int order) { return mu / std::pow(m00, 1.0 + order / 2.0); };
    const double n20 = eta(mu20, 2), n02 = eta(mu02, 2), n11 = eta(mu11, 2);
    const double n30 = eta(mu30, 3), n03 = eta(mu03, 3), n21 = eta(mu21, 3), n12 = eta(mu12, 3);
    const double hu1 = n20 + n02;
    const double hu2 = (n20 - n02) * (n20 - n02) + 4.0 * n11 * n11;
    const double hu3 =
        (n30 - 3.0 * n12) * (n30 - 3.0 * n12) + (3.0 * n21 - n03) * (3.0 * n21 - n03);

    out.centroid = {cx, cy};
    out.theta = theta;
    out.minE1 = minE1;
    out.maxE1 = maxE1;
    out.minE2 = minE2;
    out.maxE2 = maxE2;
    out.values = {percentFilled, aspectRatio, hu1, hu2, hu3};
    return true;
}

std::array<cv::Point2d, 4> orientedBox(const RegionFeatures& f) {
    const cv::Point2d u(std::cos(f.theta), std::sin(f.theta));
    const cv::Point2d v(-u.y, u.x);
    return {f.centroid + f.minE1 * u + f.minE2 * v, f.centroid + f.maxE1 * u + f.minE2 * v,
            f.centroid + f.maxE1 * u + f.maxE2 * v, f.centroid + f.minE1 * u + f.maxE2 * v};
}

}  // namespace objectrecognition
