// Live viewer: tasks 1-6 on a camera, video, or image directory.
// Usage: live [camera-index | video-file | stream-url | image-dir]   (default: camera 0)
// Keys: q/Esc quit
//       n     type a new label straight into the video window; Enter = set, Esc = cancel
//       t     train: append the primary region's features + label to the feature DB (T5)
//       s     save raw frame -> data/objects/<label>/<timestamp>.png
//             and each stage -> out/stages/<stage>/<label>/<timestamp>.png   (D-06)
//       any other key -> next still image

#include <cctype>
#include <chrono>
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

// The terminal isn't a reliable input under every shell, and switching focus away from the
// video is clumsy, so the label is typed into the window itself (D-06 fallback).
bool isLabelChar(int k) { return std::isalnum(k) || k == '_' || k == '-'; }

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
    bool editing = false;
    std::string draft;
    // Confirmations show in the window too: the terminal may be hidden behind it.
    std::string notice;
    auto noticeUntil = std::chrono::steady_clock::now();
    const auto notify = [&](const std::string& msg) {
        notice = msg;
        noticeUntil = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        std::cout << msg << "\n";
    };

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
        const std::string labelText =
            editing ? "type label: " + draft + "_  (Enter/Esc)"
                    : "label=" + (label.empty() ? std::string("<none: press n>") : label);
        const std::string status = "t=" + std::to_string(result.threshold) + "  " + labelText +
                                   "  db=" + std::to_string(db.size());
        cv::putText(display, status, {10, 30}, cv::FONT_HERSHEY_SIMPLEX, 0.7, {0, 0, 255}, 2);
        if (std::chrono::steady_clock::now() < noticeUntil)
            cv::putText(display, notice, {10, 60}, cv::FONT_HERSHEY_SIMPLEX, 0.7, {0, 160, 0}, 2);

        cv::imshow("original", display);
        cv::imshow("threshold", result.binary);
        cv::imshow("regions", regionsVis);

        const int raw = cv::waitKey(source.isStill() ? 0 : 1);
        const int key = raw < 0 ? -1 : raw & 0xFF;

        if (editing) {
            if (key == 13 || key == 10) {
                if (isValidLabel(draft)) {
                    label = draft;
                    notify("label set: " + label);
                } else {
                    notify("labels: 1-64 letters, digits, _ or -");
                }
                editing = false;
            } else if (key == 27) {
                editing = false;
            } else if (key == 8 && !draft.empty()) {
                draft.pop_back();
            } else if (key >= 0 && isLabelChar(key) && draft.size() < 64) {
                draft += static_cast<char>(key);
            }
            continue;
        }

        if (key == 'q' || key == 27) break;
        if (key == 'n') {
            editing = true;
            draft.clear();
        }
        if (key == 't') {
            if (label.empty() || !result.hasFeatures) {
                notify("need a label (n) and a detected object to train");
                continue;
            }
            const orec::Sample sample{label, result.features.values};
            appendSample(kDbPath, sample);
            db.push_back(sample);
            stdevs = orec::featureStdevs(db);
            notify("trained '" + label + "' (" + std::to_string(db.size()) + " samples)");
        }
        if (key == 's') {
            if (label.empty()) {
                notify("no label set: press n first");
                continue;
            }
            const std::string file = timestamp() + ".png";
            saveImage(rawRoot / label, file, frame);
            saveImage(stageRoot / "threshold" / label, file, result.binary);
            saveImage(stageRoot / "cleaned" / label, file, result.cleaned);
            saveImage(stageRoot / "regions" / label, file, regionsVis);
            saveImage(stageRoot / "features" / label, file, display);
            notify("saved " + (rawRoot / label / file).string());
        }
    }
    return 0;
}
