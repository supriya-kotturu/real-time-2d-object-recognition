#include "core/regions.h"

#include <algorithm>
#include <limits>
#include <opencv2/imgproc.hpp>

namespace objectrecognition {

void segmentRegions(const cv::Mat& binary, int minArea, int maxRegions, cv::Mat& regionMap,
                    std::vector<Region>& regions) {
    CV_Assert(binary.type() == CV_8UC1);

    cv::Mat labels, stats, centroids;
    const int n = cv::connectedComponentsWithStats(binary, labels, stats, centroids, 8, CV_32S);
    const auto area = [&](int label) { return stats.at<int>(label, cv::CC_STAT_AREA); };

    std::vector<int> keep;  // original labels; 0 is the background component
    for (int label = 1; label < n; ++label) {
        if (area(label) >= minArea) keep.push_back(label);
    }
    std::sort(keep.begin(), keep.end(), [&](int a, int b) { return area(a) > area(b); });
    if (static_cast<int>(keep.size()) > maxRegions) keep.resize(maxRegions);

    std::vector<int> remap(n, 0);
    regions.clear();
    for (int k = 0; k < static_cast<int>(keep.size()); ++k) {
        const int old = keep[k];
        remap[old] = k + 1;
        const cv::Rect box(
            stats.at<int>(old, cv::CC_STAT_LEFT), stats.at<int>(old, cv::CC_STAT_TOP),
            stats.at<int>(old, cv::CC_STAT_WIDTH), stats.at<int>(old, cv::CC_STAT_HEIGHT));
        const bool border =
            box.x == 0 || box.y == 0 || box.br().x >= binary.cols || box.br().y >= binary.rows;
        regions.push_back({k + 1,
                           area(old),
                           {centroids.at<double>(old, 0), centroids.at<double>(old, 1)},
                           box,
                           border});
    }

    regionMap.create(labels.size(), CV_32SC1);
    for (int r = 0; r < labels.rows; ++r) {
        const int* src = labels.ptr<int>(r);
        int* dst = regionMap.ptr<int>(r);
        for (int c = 0; c < labels.cols; ++c) dst[c] = remap[src[c]];
    }
}

int primaryRegion(const std::vector<Region>& regions, cv::Size imageSize) {
    const cv::Point2d center(imageSize.width / 2.0, imageSize.height / 2.0);
    int best = -1;
    double bestDist = std::numeric_limits<double>::max();
    for (int i = 0; i < static_cast<int>(regions.size()); ++i) {
        if (regions[i].touchesBorder) continue;
        const cv::Point2d d = regions[i].centroid - center;
        const double dist = d.dot(d);
        if (dist < bestDist) {
            bestDist = dist;
            best = i;
        }
    }
    return best;
}

}  // namespace objectrecognition
