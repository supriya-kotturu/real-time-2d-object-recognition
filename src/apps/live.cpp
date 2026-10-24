// Live viewer: tasks 1-6 on a camera, video, or image directory.
// Usage: live [camera-index | video-file | stream-url | image-dir]   (default: camera 0)
// Keys: q/Esc quit
//       n     set the current object label (typed in the terminal; video pauses meanwhile)
//       t     train: append the primary region's features + label to the feature DB (T5)
//       s     save raw frame -> data/objects/<label>/<timestamp>.png
//             and each stage -> out/stages/<stage>/<label>/<timestamp>.png   (D-06)
//       any other key -> next still image

#include <filesystem>
#include <iostream>
#include <opencv2/core/utils/logger.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

#include "app/capture.h"
#include "app/feature_db.h"
#include "app/frame_source.h"
#include "app/overlay.h"
#include "app/pipeline.h"
#include "core/classifier.h"

namespace fs = std::filesystem;
namespace orec = objectrecognition;

namespace {

const std::string kDbPath = "data/db/features.csv";
// Scaled-Euclidean distance above which the nearest match is reported as unknown.
// Placeholder until it's tuned on real captures (GUI slider later).
constexpr double kUnknownThreshold = 3.0;

}  // namespace

int main(int argc, char** argv) {
    // Silences INFO spam from OpenCV probing optional TBB/OpenMP plugins that aren't shipped.
    cv::utils::logging::setLogLevel(cv::utils::logging::LOG_LEVEL_WARNING);

    const std::string spec = argc > 1 ? argv[1] : "0";
    FrameSource source(spec);
    if (!source.isOpen()) {
        std::cerr << "cannot open input: " << spec << "\n";
        return 1;
    }

    const fs::path rawRoot = "data/objects";
    const fs::path stageRoot = "out/stages";
    std::string label;

    std::vector<orec::Sample> db = loadFeatureDb(kDbPath);
    std::vector<double> stdevs = orec::featureStdevs(db);
    std::cout << "feature DB: " << db.size() << " samples from " << kDbPath << "\n";

    FrameResult result;
    cv::Mat frame, display, regionsVis;
    while (source.next(frame)) {
        runPipeline(frame, result);

        // Overlay goes on a deep copy so saved frames stay clean report/pipeline inputs.
        frame.copyTo(display);
        colorizeRegions(result.regionMap, regionsVis);

        std::string objectText = "no object";
        if (result.hasFeatures) {
            // Live feature value for the T4 invariance check: rotate/move the object and watch.
            objectText = "fill=" + cv::format("%.2f", result.features.values[0]);
            // Stdev over a single sample is 0 for every feature, so wait for two.
            if (db.size() >= 2) {
                const auto match =
                    orec::classifyNearest(db, stdevs, result.features.values, kUnknownThreshold);
                objectText = (match.known ? match.label : "unknown (" + match.label + "?)") +
                             cv::format(" d=%.2f", match.distance);
            }
            drawFeatures(display, result.features, objectText);
        }
        const std::string status = "t=" + std::to_string(result.threshold) +
                                   "  label=" + (label.empty() ? "<none: press n>" : label) +
                                   "  db=" + std::to_string(db.size());
        cv::putText(display, status, {10, 30}, cv::FONT_HERSHEY_SIMPLEX, 0.7, {0, 0, 255}, 2);

        cv::imshow("original", display);
        cv::imshow("threshold", result.binary);
        cv::imshow("regions", regionsVis);

        const int key = cv::waitKey(source.isStill() ? 0 : 1);
        if (key == 'q' || key == 27) break;
        if (key == 'n') {
            // D-06 option (b): blocks the video loop while typing. If that proves unusable,
            // record why in decisions.md and fall back to a --label CLI arg (option a).
            std::cout << "label (click this terminal, type, Enter): " << std::flush;
            std::getline(std::cin, label);
            if (!isValidLabel(label)) {
                std::cout << "labels may only use letters, digits, _ and -\n";
                label.clear();
            }
        }
        if (key == 't') {
            if (label.empty() || !result.hasFeatures) {
                std::cout << "need a label (n) and a detected object to train\n";
                continue;
            }
            const orec::Sample sample{label, result.features.values};
            appendSample(kDbPath, sample);
            db.push_back(sample);
            stdevs = orec::featureStdevs(db);
            std::cout << "trained '" << label << "' (" << db.size() << " samples)\n";
        }
        if (key == 's') {
            if (label.empty()) {
                std::cout << "no label set: press n first\n";
                continue;
            }
            const std::string file = timestamp() + ".png";
            saveImage(rawRoot / label, file, frame);
            saveImage(stageRoot / "threshold" / label, file, result.binary);
            saveImage(stageRoot / "regions" / label, file, regionsVis);
            saveImage(stageRoot / "features" / label, file, display);
            std::cout << "saved " << (rawRoot / label / file).string() << "\n";
        }
    }
    return 0;
}
