// Extension: measure, rather than assume, how rotation/scale invariant each feature is.
// Each image's primary object is cut onto a white canvas (so clutter can't change which
// region is picked), then rotated 0..345 deg and scaled 0.5..1.5x, and every variant goes
// through the full pipeline. Drift = stdev / |mean| of each feature across the variants.
// Usage: invariance [image-dir]   (default: data/examples)
// Writes out/invariance/{samples.csv,summary.csv,rotation.png,scale.png}

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <opencv2/core/utils/logger.hpp>
#include <opencv2/geometry.hpp>  // OpenCV 5 moved getRotationMatrix2D/boundingRect here
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include "app/frame_source.h"
#include "app/pipeline.h"

namespace fs = std::filesystem;
namespace orec = objectrecognition;

namespace {

const std::vector<double> kAngles = [] {
    std::vector<double> a;
    for (int d = 0; d < 360; d += 15) a.push_back(d);
    return a;
}();
const std::vector<double> kScales = {0.5, 0.625, 0.75, 0.875, 1.0, 1.25, 1.5};

struct Series {
    std::string image;
    std::vector<double> params;
    std::vector<std::vector<double>> features;  // [variant][feature]
};

// Object pixels from the original on a white canvas with room to rotate without clipping.
cv::Mat isolateObject(const cv::Mat& frame, const FrameResult& r) {
    cv::Mat mask = (r.regionMap == r.regions[r.primary].id);
    // A few pixels of margin keep the object's own anti-aliased edge for the threshold.
    cv::dilate(mask, mask, cv::getStructuringElement(cv::MORPH_ELLIPSE, {7, 7}));
    cv::Mat canvas(frame.size(), frame.type(), cv::Scalar::all(255));
    frame.copyTo(canvas, mask);

    const cv::Rect box = cv::boundingRect(mask);
    const int pad = static_cast<int>(std::hypot(box.width, box.height));
    cv::Mat out;
    cv::copyMakeBorder(canvas(box), out, pad, pad, pad, pad, cv::BORDER_CONSTANT,
                       cv::Scalar::all(255));
    return out;
}

bool featuresOf(const cv::Mat& img, FrameResult& r, std::vector<double>& out) {
    runPipeline(img, r);
    if (!r.hasFeatures) return false;
    out = r.features.values;
    return true;
}

double relativeSpread(const std::vector<std::vector<double>>& rows, size_t f) {
    double mean = 0, var = 0;
    for (const auto& row : rows) mean += row[f];
    mean /= static_cast<double>(rows.size());
    for (const auto& row : rows) var += (row[f] - mean) * (row[f] - mean);
    const double sd = std::sqrt(var / static_cast<double>(rows.size()));
    return std::abs(mean) < 1e-12 ? 0.0 : sd / std::abs(mean);
}

// One small chart per feature: value relative to the identity transform, one line per image.
cv::Mat plot(const std::vector<Series>& series, const std::string& xName, double identity) {
    const int w = 360, h = 220, margin = 36;
    const size_t nf = orec::kFeatureNames.size();
    // One panel per feature plus a legend panel; colours repeat per panel via the same seed.
    cv::Mat img(h, w * static_cast<int>(nf + 1), CV_8UC3, cv::Scalar::all(255));
    const double yMin = 0.5, yMax = 1.5;  // ratio to the identity value

    for (size_t f = 0; f < nf; ++f) {
        const cv::Rect panel(static_cast<int>(f) * w, 0, w, h);
        cv::Mat p = img(panel);
        const double x0 = series.empty() ? 0 : series[0].params.front();
        const double x1 = series.empty() ? 1 : series[0].params.back();
        const auto toPx = [&](double x, double y) {
            const double cy = std::clamp(y, yMin, yMax);
            return cv::Point(
                margin + static_cast<int>((x - x0) / (x1 - x0) * (w - 2 * margin)),
                h - margin - static_cast<int>((cy - yMin) / (yMax - yMin) * (h - 2 * margin)));
        };
        cv::rectangle(p, {margin, margin}, {w - margin, h - margin}, cv::Scalar::all(200));
        cv::line(p, toPx(x0, 1.0), toPx(x1, 1.0), cv::Scalar::all(150), 1, cv::LINE_AA);
        cv::putText(p, orec::kFeatureNames[f], {margin, 24}, cv::FONT_HERSHEY_SIMPLEX, 0.55,
                    cv::Scalar::all(0), 1, cv::LINE_AA);
        cv::putText(p, xName, {w / 2 - 20, h - 8}, cv::FONT_HERSHEY_SIMPLEX, 0.45,
                    cv::Scalar::all(80), 1, cv::LINE_AA);
        cv::putText(p, "1.5", {4, margin + 4}, cv::FONT_HERSHEY_SIMPLEX, 0.4, cv::Scalar::all(80));
        cv::putText(p, "0.5", {4, h - margin}, cv::FONT_HERSHEY_SIMPLEX, 0.4, cv::Scalar::all(80));

        cv::RNG rng(17);
        for (const auto& s : series) {
            const auto it = std::find(s.params.begin(), s.params.end(), identity);
            if (it == s.params.end()) continue;
            const double ref = s.features[it - s.params.begin()][f];
            if (std::abs(ref) < 1e-12) continue;
            const cv::Scalar color(rng.uniform(0, 200), rng.uniform(0, 200), rng.uniform(0, 200));
            for (size_t i = 1; i < s.params.size(); ++i)
                cv::line(p, toPx(s.params[i - 1], s.features[i - 1][f] / ref),
                         toPx(s.params[i], s.features[i][f] / ref), color, 2, cv::LINE_AA);
        }
    }

    cv::RNG rng(17);
    cv::Mat legend = img(cv::Rect(static_cast<int>(nf) * w, 0, w, h));
    for (size_t i = 0; i < series.size(); ++i) {
        const cv::Scalar color(rng.uniform(0, 200), rng.uniform(0, 200), rng.uniform(0, 200));
        const int y = margin + static_cast<int>(i) * 20;
        cv::line(legend, {margin, y - 4}, {margin + 24, y - 4}, color, 3, cv::LINE_AA);
        cv::putText(legend, series[i].image, {margin + 32, y}, cv::FONT_HERSHEY_SIMPLEX, 0.5,
                    cv::Scalar::all(0), 1, cv::LINE_AA);
    }
    return img;
}

}  // namespace

int main(int argc, char** argv) {
    cv::utils::logging::setLogLevel(cv::utils::logging::LOG_LEVEL_WARNING);

    const std::string dir = argc > 1 ? argv[1] : "data/examples";
    FrameSource source(dir);
    if (!source.isOpen() || !source.isStill()) {
        std::cerr << "not an image directory: " << dir << "\n";
        return 1;
    }

    const fs::path out = "out/invariance";
    fs::create_directories(out);
    std::ofstream samples(out / "samples.csv");
    samples << "image,transform,param";
    for (const auto& n : orec::kFeatureNames) samples << ',' << n;
    samples << '\n';

    std::vector<Series> rotations, scales;
    FrameResult r;
    cv::Mat frame, variant;
    while (source.next(frame)) {
        runPipeline(frame, r);
        if (!r.hasFeatures) {
            std::cout << "skip (no object): " << source.currentName() << "\n";
            continue;
        }
        const cv::Mat object = isolateObject(frame, r);
        Series rot{source.currentName(), {}, {}}, scl{source.currentName(), {}, {}};
        std::vector<double> f;

        const cv::Point2f center(object.cols / 2.0f, object.rows / 2.0f);
        for (double a : kAngles) {
            const cv::Mat m = cv::getRotationMatrix2D(center, a, 1.0);
            cv::warpAffine(object, variant, m, object.size(), cv::INTER_LINEAR, cv::BORDER_CONSTANT,
                           cv::Scalar::all(255));
            if (!featuresOf(variant, r, f)) continue;
            rot.params.push_back(a);
            rot.features.push_back(f);
        }
        for (double s : kScales) {
            cv::resize(object, variant, {}, s, s, s < 1 ? cv::INTER_AREA : cv::INTER_LINEAR);
            if (!featuresOf(variant, r, f)) continue;
            scl.params.push_back(s);
            scl.features.push_back(f);
        }

        for (const auto* series : {&rot, &scl}) {
            const char* kind = series == &rot ? "rotate" : "scale";
            for (size_t i = 0; i < series->params.size(); ++i) {
                samples << series->image << ',' << kind << ',' << series->params[i];
                for (double v : series->features[i]) samples << ',' << v;
                samples << '\n';
            }
        }
        if (rot.params.size() > 1) rotations.push_back(std::move(rot));
        if (scl.params.size() > 1) scales.push_back(std::move(scl));
    }
    if (rotations.empty()) {
        std::cerr << "no usable images\n";
        return 1;
    }

    // Mean relative spread over images, per feature: lower = more invariant.
    std::ofstream summary(out / "summary.csv");
    summary << "feature,rotation_drift_pct,scale_drift_pct\n";
    std::cout << "\nfeature drift (stdev/|mean| across variants, averaged over " << rotations.size()
              << " images)\n\n| feature | rotation | scale |\n|---|---|---|\n";
    for (size_t f = 0; f < orec::kFeatureNames.size(); ++f) {
        double rotDrift = 0, sclDrift = 0;
        for (const auto& s : rotations) rotDrift += relativeSpread(s.features, f);
        for (const auto& s : scales) sclDrift += relativeSpread(s.features, f);
        rotDrift = 100 * rotDrift / static_cast<double>(rotations.size());
        sclDrift = scales.empty() ? 0 : 100 * sclDrift / static_cast<double>(scales.size());
        summary << orec::kFeatureNames[f] << ',' << rotDrift << ',' << sclDrift << '\n';
        std::cout << "| " << orec::kFeatureNames[f] << " | " << std::fixed << std::setprecision(1)
                  << rotDrift << "% | " << sclDrift << "% |\n";
    }

    cv::imwrite((out / "rotation.png").string(), plot(rotations, "angle (deg)", 0.0));
    cv::imwrite((out / "scale.png").string(), plot(scales, "scale", 1.0));
    std::cout << "\nwrote " << out.string()
              << "/{samples.csv,summary.csv,rotation.png,scale.png}\n";
    return 0;
}
