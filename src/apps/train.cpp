// T5 batch mode: rebuild the feature DB from labelled still images, the alternative the task
// allows to key-press training. Since raw captures are the source of truth (D-06), the DB can
// be regenerated whenever the feature set changes (D-12).
// Usage: train [objects-dir] [db-path]   (default: data/objects data/db/features.csv)

#include <iostream>
#include <map>
#include <opencv2/core/utils/logger.hpp>
#include <opencv2/imgcodecs.hpp>

#include "app/dataset.h"
#include "app/feature_db.h"
#include "app/pipeline.h"

namespace orec = objectrecognition;

int main(int argc, char** argv) {
    cv::utils::logging::setLogLevel(cv::utils::logging::LOG_LEVEL_WARNING);

    const std::string root = argc > 1 ? argv[1] : "data/objects";
    const std::string dbPath = argc > 2 ? argv[2] : "data/db/features.csv";
    const auto images = listLabeledImages(root);
    if (images.empty()) {
        std::cerr << "no <label>/<image> files under " << root << "\n";
        return 1;
    }

    std::vector<orec::Sample> db;
    std::map<std::string, int> perLabel;
    FrameResult result;
    for (const auto& img : images) {
        const cv::Mat frame = cv::imread(img.path.string(), cv::IMREAD_COLOR);
        if (frame.empty()) continue;
        runPipeline(frame, result);
        if (!result.hasFeatures) {
            std::cout << "skip (no object): " << img.path.string() << "\n";
            continue;
        }
        db.push_back({img.label, result.features.values});
        ++perLabel[img.label];
    }

    saveFeatureDb(dbPath, db);
    for (const auto& [label, n] : perLabel) std::cout << label << ": " << n << "\n";
    std::cout << "wrote " << db.size() << " samples to " << dbPath << "\n";
    return 0;
}
