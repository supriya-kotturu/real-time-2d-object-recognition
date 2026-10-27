// Checks T9 steps A-C (D-18): a hammer-shaped object rotated to every angle must come back as
// the same upright ROI. A wrong rotation sign or an unresolved 180 deg flip shows up as a large
// pixel difference against the 0 deg ROI. With the model present, also checks the embedding.
// Usage: embedding_tests [model.onnx]   (default: data/models/resnet18-v2-7.onnx)

#include <filesystem>
#include <iostream>
#include <opencv2/core/utils/logger.hpp>
#include <opencv2/geometry.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include "app/embedding.h"
#include "app/pipeline.h"
#include "core/classifier.h"

namespace orec = objectrecognition;

namespace {

int failures = 0;

void expect(bool ok, const std::string& what) {
    std::cout << (ok ? "[ OK ] " : "[FAIL] ") << what << "\n";
    if (!ok) ++failures;
}

// Dark hammer on white: a handle with a head at one end. Symmetric across its own axis (a
// rotation can't undo a mirror image) but not end-to-end, so the 180 deg flip is observable.
cv::Mat hammer() {
    cv::Mat img(480, 640, CV_8UC3, cv::Scalar::all(240));
    cv::rectangle(img, cv::Rect(220, 228, 200, 24), cv::Scalar(40, 30, 30), cv::FILLED);
    cv::rectangle(img, cv::Rect(400, 195, 40, 90), cv::Scalar(40, 30, 30), cv::FILLED);
    return img;
}

bool roiAt(const cv::Mat& base, double angle, cv::Mat& roi) {
    cv::Mat rotated;
    const cv::Mat m = cv::getRotationMatrix2D(cv::Point2f(320, 240), angle, 1.0);
    cv::warpAffine(base, rotated, m, base.size(), cv::INTER_LINEAR, cv::BORDER_CONSTANT,
                   cv::Scalar::all(240));
    FrameResult r;
    runPipeline(rotated, r);
    if (!r.hasFeatures) return false;
    return extractAlignedRoi(rotated, r.regionMap, r.regions[r.primary].id, r.features, roi);
}

double pixelDifference(const cv::Mat& a, const cv::Mat& b) {
    cv::Mat ga, gb;
    cv::resize(a, ga, {64, 64}, 0, 0, cv::INTER_AREA);
    cv::resize(b, gb, {64, 64}, 0, 0, cv::INTER_AREA);
    return cv::norm(ga, gb, cv::NORM_L1) / static_cast<double>(ga.total() * ga.channels());
}

}  // namespace

int main(int argc, char** argv) {
    cv::utils::logging::setLogLevel(cv::utils::logging::LOG_LEVEL_WARNING);
    const cv::Mat base = hammer();

    cv::Mat reference;
    expect(roiAt(base, 0, reference), "ROI extracted at 0 deg");
    expect(reference.cols > reference.rows, "ROI is landscape (primary axis along x)");
    std::filesystem::create_directories("out/tests");
    cv::imwrite("out/tests/roi_0deg.png", reference);

    double worst = 0;
    int worstAngle = 0;
    for (int angle = 15; angle < 360; angle += 15) {
        cv::Mat roi;
        if (!roiAt(base, angle, roi)) {
            expect(false, "ROI extracted at " + std::to_string(angle) + " deg");
            continue;
        }
        const double diff = pixelDifference(reference, roi);
        if (diff > worst) {
            worst = diff;
            worstAngle = angle;
            cv::imwrite("out/tests/roi_worst.png", roi);
        }
    }
    // Interpolation and pixel-grid effects stay well under ~10 grey levels; a flipped or
    // misrotated ROI differs by ~50+ because the head lands on the wrong end.
    expect(worst < 12.0, cv::format("all 23 rotations match the 0 deg ROI (worst %.1f at %d deg)",
                                    worst, worstAngle));

    const std::string modelPath = argc > 1 ? argv[1] : "data/models/resnet18-v2-7.onnx";
    if (!std::filesystem::exists(modelPath)) {
        std::cout << "[SKIP] embedding checks: " << modelPath << " not found\n";
    } else {
        Embedder embedder(modelPath);
        cv::Mat rotatedRoi;
        roiAt(base, 135, rotatedRoi);
        std::vector<double> e0, e1;
        embedder.embed(reference, e0);
        embedder.embed(rotatedRoi, e1);
        expect(e0.size() == 512, "embedding has 512 dimensions");
        const double d = orec::cosineDistance(e0, e1);
        expect(d < 0.05, cv::format("same object at 0 and 135 deg: cosine distance %.4f", d));

        cv::Mat blank(reference.size(), reference.type(), cv::Scalar::all(240));
        cv::circle(blank, {blank.cols / 2, blank.rows / 2}, blank.rows / 3, cv::Scalar(40, 30, 30),
                   cv::FILLED);
        std::vector<double> e2;
        embedder.embed(blank, e2);
        const double other = orec::cosineDistance(e0, e2);
        expect(other > d,
               cv::format("a disc is farther from the hammer (%.4f) than its rotation", other));
    }

    std::cout << "\n" << failures << " failed\n";
    return failures == 0 ? 0 : 1;
}
