#pragma once

#include <opencv2/core.hpp>
#include <vector>

namespace objectrecognition {

struct Region {
    int id;  // label in the renumbered region map (1..N)
    int area;
    cv::Point2d centroid;
    cv::Rect bbox;  // axis-aligned
    bool touchesBorder;
};

// T3 via OpenCV connected components (D-09). Keeps regions with area >= minArea, largest
// first, at most maxRegions, renumbered 1..N in regionMap (CV_32S, 0 = background) so a
// fixed palette maps to stable-ish colours.
void segmentRegions(const cv::Mat& binary, int minArea, int maxRegions, cv::Mat& regionMap,
                    std::vector<Region>& regions);

// Task 3 starting heuristic: the most central region that doesn't touch the image border.
// Returns an index into regions, or -1 if every region touches the border.
int primaryRegion(const std::vector<Region>& regions, cv::Size imageSize);

}  // namespace objectrecognition
