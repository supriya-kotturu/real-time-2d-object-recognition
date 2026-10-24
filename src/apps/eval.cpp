// T7 + T9 comparison: confusion matrices over labelled images (data/objects/<label>/*.png).
// Usage: eval [objects-dir] [--one-shot | --db features.csv] [--unknown X] [--model m.onnx]
//
// Protocols (D-14):
//   default     leave-one-out: each image vs a DB of every *other* image, so all captures are
//               test data and nothing is matched against itself.
//   --one-shot  Task 9: the first image of each label is the whole DB; the rest are tests.
//   --db        every image vs a fixed hand-feature DB (e.g. trained interactively in live).
// When the ResNet18 model exists, the embedding classifier (cosine) runs on the same split,
// so the two methods are compared on identical data.
// Writes out/eval/confusion_{features,embedding}.{md,csv} and lists misclassified files.

#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <opencv2/core/utils/logger.hpp>
#include <opencv2/imgcodecs.hpp>

#include "app/dataset.h"
#include "app/embedding.h"
#include "app/feature_db.h"
#include "app/pipeline.h"
#include "core/classifier.h"
#include "core/evaluation.h"

namespace fs = std::filesystem;
namespace orec = objectrecognition;

namespace {

struct Item {
    std::string label, file;
    std::vector<double> features, embedding;
};

using Classify =
    std::function<orec::Match(const std::vector<orec::Sample>&, const std::vector<double>&)>;
using Pick = std::function<const std::vector<double>&(const Item&)>;

void report(const std::string& name, const orec::ConfusionMatrix& m,
            const std::vector<std::string>& mistakes) {
    const fs::path out = "out/eval";
    fs::create_directories(out);
    std::ofstream(out / ("confusion_" + name + ".md")) << m.toMarkdown();
    std::ofstream(out / ("confusion_" + name + ".csv")) << m.toCsv();
    std::cout << "\n## " << name << "\n\n" << m.toMarkdown();
    for (const auto& s : mistakes) std::cout << "  miss: " << s << "\n";
}

void run(const std::string& name, const std::vector<Item>& items, const std::string& protocol,
         const std::vector<orec::Sample>& fixedDb, const Pick& pick, const Classify& classify) {
    orec::ConfusionMatrix matrix;
    std::vector<std::string> mistakes;

    // One-shot: the first image of each label (items are sorted) forms the DB.
    std::vector<orec::Sample> oneShotDb;
    std::map<std::string, size_t> exemplar;
    for (size_t i = 0; i < items.size(); ++i)
        if (exemplar.emplace(items[i].label, i).second)
            oneShotDb.push_back({items[i].label, pick(items[i])});

    for (size_t i = 0; i < items.size(); ++i) {
        std::vector<orec::Sample> db;
        if (protocol == "loo") {
            for (size_t j = 0; j < items.size(); ++j)
                if (j != i) db.push_back({items[j].label, pick(items[j])});
        } else if (protocol == "one-shot") {
            if (exemplar[items[i].label] == i) continue;  // DB images aren't tested
            db = oneShotDb;
        } else {
            db = fixedDb;
        }
        const orec::Match match = classify(db, pick(items[i]));
        const std::string predicted = match.known ? match.label : "unknown";
        matrix.add(items[i].label, predicted);
        if (predicted != items[i].label)
            mistakes.push_back(items[i].file + "  (" + items[i].label + " -> " + predicted +
                               cv::format(", d=%.3f)", match.distance));
    }
    report(name, matrix, mistakes);
}

}  // namespace

int main(int argc, char** argv) {
    cv::utils::logging::setLogLevel(cv::utils::logging::LOG_LEVEL_WARNING);

    std::string root = "data/objects", dbPath, modelPath = "data/models/resnet18-v2-7.onnx";
    std::string protocol = "loo";
    double unknownThreshold = std::numeric_limits<double>::infinity();
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--db" && i + 1 < argc) {
            dbPath = argv[++i];
            protocol = "db";
        } else if (arg == "--one-shot") {
            protocol = "one-shot";
        } else if (arg == "--unknown" && i + 1 < argc) {
            unknownThreshold = std::stod(argv[++i]);
        } else if (arg == "--model" && i + 1 < argc) {
            modelPath = argv[++i];
        } else {
            root = arg;
        }
    }

    std::unique_ptr<Embedder> embedder;
    if (fs::exists(modelPath))
        embedder = std::make_unique<Embedder>(modelPath);
    else
        std::cout << "(no model at " << modelPath << ": embedding comparison skipped)\n";

    std::vector<Item> items;
    FrameResult r;
    cv::Mat roi;
    for (const auto& img : listLabeledImages(root)) {
        const cv::Mat frame = cv::imread(img.path.string(), cv::IMREAD_COLOR);
        if (frame.empty()) continue;
        runPipeline(frame, r);
        if (!r.hasFeatures) {
            std::cout << "skip (no object): " << img.path.string() << "\n";
            continue;
        }
        Item item{img.label, img.path.string(), r.features.values, {}};
        if (embedder &&
            extractAlignedRoi(frame, r.regionMap, r.regions[r.primary].id, r.features, roi))
            embedder->embed(roi, item.embedding);
        items.push_back(std::move(item));
    }
    if (items.size() < 3) {
        std::cerr << "need at least 3 usable images under " << root << "\n";
        return 1;
    }

    std::vector<orec::Sample> fixedDb;
    if (protocol == "db") {
        fixedDb = loadFeatureDb(dbPath);
        if (fixedDb.size() < 2) {
            std::cerr << "DB " << dbPath << " needs at least 2 samples\n";
            return 1;
        }
    }
    std::cout << "protocol: " << protocol << ", " << items.size() << " images\n";

    run(
        "features", items, protocol, fixedDb,
        [](const Item& it) -> const std::vector<double>& { return it.features; },
        [&](const std::vector<orec::Sample>& db, const std::vector<double>& f) {
            return orec::classifyNearest(db, orec::featureStdevs(db), f, unknownThreshold);
        });

    // A fixed hand-feature DB has no embeddings, so that protocol compares features only.
    if (embedder && protocol != "db") {
        run(
            "embedding", items, protocol, {},
            [](const Item& it) -> const std::vector<double>& { return it.embedding; },
            [&](const std::vector<orec::Sample>& db, const std::vector<double>& e) {
                return orec::classifyCosine(db, e, unknownThreshold);
            });
    }
    std::cout << "\nwrote out/eval/confusion_*.{md,csv}\n";
    return 0;
}
