// Headless batch run of tasks 1-4 over an image directory. Produces the report's required
// images and feature vectors without clicking through windows.
// Usage: inspect [image-dir]   (default: data/examples)
// Writes out/stages/{threshold,cleaned,regions,features,roi}/<stem>.png and out/stages/features.csv
// (roi = Task 9 steps A-C: the original image rotated so the primary axis lies along x).
// With a trained DB it also writes out/stages/classified/<stem>.png (Task 6: the predicted label
// at the object, both classifiers in a caption) and out/stages/predictions.csv.

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <opencv2/core/utils/logger.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include "app/embedding.h"
#include "app/feature_db.h"
#include "core/classifier.h"
#include "app/frame_source.h"
#include "app/overlay.h"
#include "app/pipeline.h"

namespace fs = std::filesystem;
namespace orec = objectrecognition;

namespace {

const std::string kModelPath = "data/models/resnet18-v2-7.onnx";
constexpr double kFeatureCutoff = 3.0;  // scaled Euclidean; GUI default (D-25)
constexpr double kCosineCutoff = 0.35;

std::string predicted(const orec::Match& m) { return m.known ? m.label : "unknown"; }

// Dark band along the bottom so the caption reads on any image, in the overlay's lime.
void captionBar(cv::Mat& img, const std::string& text) {
    const int h = 30;
    cv::rectangle(img, {0, img.rows - h, img.cols, h}, cv::Scalar(12, 15, 11), cv::FILLED);
    cv::putText(img, text, {8, img.rows - 10}, cv::FONT_HERSHEY_SIMPLEX, 0.5,
                cv::Scalar(107, 255, 124), 1, cv::LINE_AA);
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

    const fs::path out = "out/stages";
    for (const char* stage : {"threshold", "cleaned", "regions", "features", "roi"})
        fs::create_directories(out / stage);
    std::ofstream csv(out / "features.csv");
    csv << "image";
    for (const auto& name : orec::kFeatureNames) csv << ',' << name;
    csv << '\n';

    // T6: classify against the trained DBs when they exist (features always, ResNet18 if the
    // model is present), with the same cutoffs the GUI starts with.
    const auto db = loadFeatureDb("data/db/features.csv");
    const auto stdevs = orec::featureStdevs(db);
    const auto embedDb = loadFeatureDb("data/db/embeddings_resnet18.csv");
    std::unique_ptr<Embedder> embedder;
    if (fs::exists(kModelPath)) embedder = std::make_unique<Embedder>(kModelPath);
    std::vector<double> embedding;
    fs::create_directories(out / "classified");
    std::ofstream predictions(out / "predictions.csv");
    predictions << "image,features_label,features_distance,resnet18_label\n";

    FrameResult result;
    cv::Mat frame, regionsVis, display, roi, classified;
    while (source.next(frame)) {
        runPipeline(frame, result);
        const std::string file = source.currentName() + ".png";

        colorizeRegions(result.regionMap, regionsVis);
        frame.copyTo(display);
        if (result.hasFeatures) drawFeatures(display, result.features, source.currentName());

        cv::imwrite((out / "threshold" / file).string(), result.binary);
        cv::imwrite((out / "cleaned" / file).string(), result.cleaned);
        cv::imwrite((out / "regions" / file).string(), regionsVis);
        cv::imwrite((out / "features" / file).string(), display);

        std::cout << std::left << std::setw(14) << source.currentName() << " t=" << std::setw(4)
                  << result.threshold << " regions=" << result.regions.size();
        if (!result.hasFeatures) {
            std::cout << "  (no central region off the border)\n";
            continue;
        }
        const bool hasRoi = extractAlignedRoi(frame, result.regionMap,
                                              result.regions[result.primary].id, result.features, roi);
        if (hasRoi) cv::imwrite((out / "roi" / file).string(), roi);

        // T6 result image: the predicted label at the object, both classifiers in a caption.
        if (db.size() >= 2) {
            const auto byFeatures =
                orec::classifyNearest(db, stdevs, result.features.values, kFeatureCutoff);
            std::string caption = cv::format("hand features: %s (d=%.2f)",
                                             predicted(byFeatures).c_str(), byFeatures.distance);
            std::string resnet = "-";
            if (embedder && hasRoi && embedDb.size() >= 2) {
                embedder->embed(roi, embedding);
                const auto byEmbedding = orec::classifyCosine(embedDb, embedding, kCosineCutoff);
                resnet = predicted(byEmbedding);
                caption += cv::format("   ResNet18: %s (d=%.3f)", resnet.c_str(),
                                      byEmbedding.distance);
            }
            frame.copyTo(classified);
            drawFeatures(classified, result.features, predicted(byFeatures));
            captionBar(classified, caption);
            cv::imwrite((out / "classified" / file).string(), classified);
            predictions << source.currentName() << ',' << predicted(byFeatures) << ','
                        << byFeatures.distance << ',' << resnet << '\n';
        }
        csv << source.currentName();
        std::cout << "  ";
        for (size_t i = 0; i < result.features.values.size(); ++i) {
            csv << ',' << result.features.values[i];
            std::cout << orec::kFeatureNames[i] << '=' << std::setprecision(4)
                      << result.features.values[i] << ' ';
        }
        csv << '\n';
        std::cout << "\n";
    }
    std::cout << "wrote " << out.string()
              << "/{threshold,cleaned,regions,features,roi}/ and features.csv\n";
    return 0;
}
