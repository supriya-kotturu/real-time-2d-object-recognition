#include "core/preprocess.h"

#include <opencv2/imgproc.hpp>

namespace objectrecognition {

void saturationDarken(const cv::Mat& bgr, double strength, cv::Mat& dst) {
    CV_Assert(bgr.type() == CV_8UC3);
    cv::Mat hsv;
    cv::cvtColor(bgr, hsv, cv::COLOR_BGR2HSV);

    dst.create(bgr.size(), CV_8UC1);
    for (int r = 0; r < hsv.rows; ++r) {
        const cv::Vec3b* in = hsv.ptr<cv::Vec3b>(r);
        uchar* out = dst.ptr<uchar>(r);
        for (int c = 0; c < hsv.cols; ++c) {
            const double s = in[c][1] / 255.0;
            out[c] = cv::saturate_cast<uchar>(in[c][2] * (1.0 - strength * s));
        }
    }
}

}  // namespace objectrecognition
