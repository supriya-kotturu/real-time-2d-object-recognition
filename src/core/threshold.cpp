

#include "core/threshold.h"

#include <cmath>

namespace objectrecognition {

int isodataThreshold(const cv::Mat& gray) {
    const int kSampleStep = 4;
    const double kEpsilon = 0.5;
    const int kMaxIter = 15;

    // ISODATA: k-means with K=2 on intensities. t = midpoint of the dark and light means.
    // Input is single-channel and prepared by the caller, so it may be plain grayscale or a
    // saturation-adjusted image (Task 1 pre-processing); this function never assumes BGR.
    // A 4x4 sampling stride (1/16 of pixels) is enough to estimate two means and keeps this
    // cheap at 30 fps.
    // An empty cluster (e.g. blank paper) keeps its previous mean instead of dividing by zero:
    // D stays near 0, t lands between 0 and the paper, so the frame comes out all background.

    CV_Assert(gray.type() == CV_8UC1);
    double prevD = 0.0, prevL = 0.0;
    double D = 0.0, L = 255.0 / 2.0;
    int itr = 0;

    while ((std::abs(D - prevD) > kEpsilon || std::abs(L - prevL) > kEpsilon) && itr < kMaxIter) {
        double sumD = 0.0, sumL = 0.0, countD = 0.0, countL = 0.0;

        for (int r = 0; r < gray.rows; r += kSampleStep) {
            const uchar* row = gray.ptr<uchar>(r);

            for (int c = 0; c < gray.cols; c += kSampleStep) {
                const uchar px = row[c];

                if (std::abs(px - L) < std::abs(px - D)) {
                    sumL += px;
                    countL++;
                } else {
                    sumD += px;
                    countD++;
                }
            }
        }

        prevD = D;
        prevL = L;

        if (countD > 0) {
            D = sumD / countD;
        }

        if (countL > 0) {
            L = sumL / countL;
        }

        itr++;
    }

    return int((D + L) / 2.0);
}

void applyThreshold(const cv::Mat& gray, int t, cv::Mat& dst) {
    CV_Assert(gray.type() == CV_8UC1);

    dst.create(gray.size(), gray.type());

    for (int r = 0; r < gray.rows; r++) {
        const uchar* row = gray.ptr<uchar>(r);
        uchar* dstRow = dst.ptr<uchar>(r);

        for (int c = 0; c < gray.cols; c++) {
            const uchar p = row[c];
            dstRow[c] = (p > t) ? 0 : 255;
        }
    }
}

}  // namespace objectrecognition
