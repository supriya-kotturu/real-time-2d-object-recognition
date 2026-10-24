// T7: confusion matrix over labelled images (data/objects/<label>/*.png).
// Usage: eval [objects-dir] [--db path] [--unknown X]
//
// Default protocol is leave-one-out: each image is classified against a DB built from every
// *other* image, so all captures serve as test data without a separate test split and no
// image is ever matched against itself. --db instead classifies every image against a fixed
// DB (e.g. one trained interactively in live). --unknown enables the unknown-object cutoff.
// Writes out/eval/confusion.{md,csv} and lists misclassified files for the report.

#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <opencv2/core/utils/logger.hpp>
#include <opencv2/imgcodecs.hpp>

#include "app/dataset.h"
#include "app/feature_db.h"
#include "app/pipeline.h"
#include "core/classifier.h"
#include "core/evaluation.h"

namespace fs = std::filesystem;
namespace orec = objectrecognition;

int main(int argc, char** argv) {
    cv::utils::logging::setLogLevel(cv::utils::logging::LOG_LEVEL_WARNING);

    std::string root = "data/objects", dbPath;
    double unknownThreshold = std::numeric_limits<double>::infinity();
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--db" && i + 1 < argc)
            dbPath = argv[++i];
        else if (arg == "--unknown" && i + 1 < argc)
            unknownThreshold = std::stod(argv[++i]);
        else
            root = arg;
    }

    std::vector<orec::Sample> samples;
    std::vector<std::string> files;
    FrameResult result;
    for (const auto& img : listLabeledImages(root)) {
        const cv::Mat frame = cv::imread(img.path.string(), cv::IMREAD_COLOR);
        if (frame.empty()) continue;
        runPipeline(frame, result);
        if (!result.hasFeatures) {
            std::cout << "skip (no object): " << img.path.string() << "\n";
            continue;
        }
        samples.push_back({img.label, result.features.values});
        files.push_back(img.path.string());
    }
    if (samples.size() < 3) {
        std::cerr << "need at least 3 usable images under " << root << "\n";
        return 1;
    }

    const bool leaveOneOut = dbPath.empty();
    const std::vector<orec::Sample> fixedDb = leaveOneOut ? std::vector<orec::Sample>{}
                                                          : loadFeatureDb(dbPath);
    if (!leaveOneOut && fixedDb.size() < 2) {
        std::cerr << "DB " << dbPath << " needs at least 2 samples\n";
        return 1;
    }
    const auto fixedStdevs = orec::featureStdevs(fixedDb);

    orec::ConfusionMatrix matrix;
    std::vector<std::string> mistakes;
    for (size_t i = 0; i < samples.size(); ++i) {
        orec::Match match;
        if (leaveOneOut) {
            std::vector<orec::Sample> db;
            for (size_t j = 0; j < samples.size(); ++j)
                if (j != i) db.push_back(samples[j]);
            match = orec::classifyNearest(db, orec::featureStdevs(db), samples[i].features,
                                          unknownThreshold);
        } else {
            match = orec::classifyNearest(fixedDb, fixedStdevs, samples[i].features,
                                          unknownThreshold);
        }
        const std::string predicted = match.known ? match.label : "unknown";
        matrix.add(samples[i].label, predicted);
        if (predicted != samples[i].label)
            mistakes.push_back(files[i] + "  (" + samples[i].label + " -> " + predicted +
                               cv::format(", d=%.2f)", match.distance));
    }

    const fs::path out = "out/eval";
    fs::create_directories(out);
    std::ofstream(out / "confusion.md") << matrix.toMarkdown();
    std::ofstream(out / "confusion.csv") << matrix.toCsv();

    std::cout << "\nprotocol: " << (leaveOneOut ? "leave-one-out" : "fixed DB " + dbPath)
              << "\n\n"
              << matrix.toMarkdown() << "\n";
    if (!mistakes.empty()) {
        std::cout << "misclassified:\n";
        for (const auto& m : mistakes) std::cout << "  " << m << "\n";
    }
    std::cout << "wrote " << (out / "confusion.md").string() << " and confusion.csv\n";
    return 0;
}
