#include "app/overlay.h"

#include <cmath>
#include <opencv2/imgproc.hpp>
#include <vector>

namespace {

// Fixed seed: region id k gets the same colour every frame and every run.
const std::vector<cv::Vec3b>& palette() {
    static const std::vector<cv::Vec3b> colors = [] {
        cv::RNG rng(5330);
        std::vector<cv::Vec3b> p(256);
        p[0] = {0, 0, 0};
        for (size_t i = 1; i < p.size(); ++i)
            p[i] = {static_cast<uchar>(rng.uniform(60, 256)),
                    static_cast<uchar>(rng.uniform(60, 256)),
                    static_cast<uchar>(rng.uniform(60, 256))};
        return p;
    }();
    return colors;
}

}  // namespace

void colorizeRegions(const cv::Mat& regionMap, cv::Mat& dst) {
    CV_Assert(regionMap.type() == CV_32SC1);
    const auto& colors = palette();
    dst.create(regionMap.size(), CV_8UC3);
    for (int r = 0; r < regionMap.rows; ++r) {
        const int* src = regionMap.ptr<int>(r);
        cv::Vec3b* out = dst.ptr<cv::Vec3b>(r);
        for (int c = 0; c < regionMap.cols; ++c) out[c] = colors[src[c] % colors.size()];
    }
}

void drawFeatures(cv::Mat& img, const objectrecognition::RegionFeatures& f,
                  const std::string& text) {
    const auto corners = objectrecognition::orientedBox(f);
    for (int i = 0; i < 4; ++i)
        cv::line(img, corners[i], corners[(i + 1) % 4], {0, 255, 0}, 2, cv::LINE_AA);

    const cv::Point2d u(std::cos(f.theta), std::sin(f.theta));
    cv::line(img, f.centroid + f.minE1 * u, f.centroid + f.maxE1 * u, {255, 0, 0}, 2, cv::LINE_AA);
    cv::circle(img, f.centroid, 4, {0, 0, 255}, cv::FILLED);
    cv::putText(img, text, f.centroid + cv::Point2d(10, -10), cv::FONT_HERSHEY_SIMPLEX, 0.7,
                {0, 0, 255}, 2);
}
