// T5 batch mode: rebuild the feature DB from labelled still images, the alternative the task
// allows to key-press training. Since raw captures are the source of truth (D-06), the DB can
// be regenerated whenever the feature set changes (D-12).
// Usage: train [objects-dir] [db-path] [--embed] [--model m.onnx]
//        (default: data/objects data/db/features.csv)
// --embed also rebuilds the ResNet18 embedding DB (D-25) next to the feature DB. It's a
// separate file because embeddings from different models can't be compared (H-01).

#include <filesystem>
#include <iostream>
#include <map>
#include <memory>
#include <opencv2/core/utils/logger.hpp>
#include <opencv2/imgcodecs.hpp>

#include "app/dataset.h"
#include "app/embedding.h"
#include "app/feature_db.h"
#include "app/pipeline.h"

namespace fs = std::filesystem;
namespace orec = objectrecognition;

int main(int argc, char** argv) {
    cv::utils::logging::setLogLevel(cv::utils::logging::LOG_LEVEL_WARNING);

    std::vector<std::string> positional;
    bool embed = false;
    std::string modelPath = "data/models/resnet18-v2-7.onnx";
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--embed")
            embed = true;
        else if (arg == "--model" && i + 1 < argc)
            modelPath = argv[++i];
        else
            positional.push_back(arg);
    }
    const std::string root = positional.size() > 0 ? positional[0] : "data/objects";
    const std::string dbPath = positional.size() > 1 ? positional[1] : "data/db/features.csv";
    const std::string embedPath =
        (fs::path(dbPath).parent_path() / "embeddings_resnet18.csv").string();

    const auto images = listLabeledImages(root);
    if (images.empty()) {
        std::cerr << "no <label>/<image> files under " << root << "\n";
        return 1;
    }
    std::unique_ptr<Embedder> embedder;
    if (embed) {
        if (!fs::exists(modelPath)) {
            std::cerr << "--embed needs the model at " << modelPath
                      << " (see data/models/README.md)\n";
            return 1;
        }
        embedder = std::make_unique<Embedder>(modelPath);
    }

    std::vector<orec::Sample> db, embeddings;
    std::map<std::string, int> perLabel;
    FrameResult result;
    cv::Mat roi;
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
        if (embedder && extractAlignedRoi(frame, result.regionMap,
                                          result.regions[result.primary].id, result.features, roi)) {
            embeddings.push_back({img.label, {}});
            embedder->embed(roi, embeddings.back().features);
        }
    }

    saveFeatureDb(dbPath, db);
    for (const auto& [label, n] : perLabel) std::cout << label << ": " << n << "\n";
    std::cout << "wrote " << db.size() << " samples to " << dbPath << "\n";
    if (embedder) {
        saveFeatureDb(embedPath, embeddings);
        std::cout << "wrote " << embeddings.size() << " embeddings to " << embedPath << "\n";
    }
    return 0;
}
