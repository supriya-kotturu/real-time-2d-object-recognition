// T7 + T9 comparison: confusion matrices over labelled images (data/objects/<label>/*.png).
// Usage: eval [test-dir] [--train dir] [--one-shot | --db features.csv] [--unknown X]
//             [--model m.onnx]
//
// Protocols (D-14):
//   default     leave-one-out: each image vs a DB of every *other* image, so all captures are
//               test data and nothing is matched against itself.
//   --one-shot  Task 9: the first image of each label is the whole DB; the rest are tests.
//   --db        every image vs a fixed hand-feature DB (e.g. trained interactively in live).
//   --train     Task 7/9 split: the DB comes from a separate training folder (every image, or
//               with --one-shot just the first per label); every test-dir image is a test.
// When the ResNet18 model exists, the embedding classifier runs on the same split with both
// cosine and SSD (D-25), so all methods are compared on identical data. Each run also prints
// the nearest-match distances of correct predictions, the evidence for the GUI's default
// unknown cutoffs.
// Writes out/eval/confusion_<method>_<protocol>.{md,csv} and lists misclassified files.

#include <algorithm>
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

void report(const std::string& name, const std::string& protocol, const orec::ConfusionMatrix& m,
            const std::vector<std::string>& mistakes) {
    const fs::path out = "out/eval";
    fs::create_directories(out);
    const std::string stem = "confusion_" + name + "_" + protocol;
    std::ofstream(out / (stem + ".md")) << m.toMarkdown();
    std::ofstream(out / (stem + ".csv")) << m.toCsv();
    std::cout << "\n## " << name << " (" << protocol << ")\n\n" << m.toMarkdown();
    for (const auto& s : mistakes) std::cout << "  miss: " << s << "\n";
}

void run(const std::string& name, const std::vector<Item>& items, const std::vector<Item>& train,
         const std::string& protocol, const std::vector<orec::Sample>& fixedDb, const Pick& pick,
         const Classify& classify) {
    orec::ConfusionMatrix matrix;
    std::vector<std::string> mistakes;
    std::vector<double> correctDistances;

    // One-shot: the first image of each label (items are sorted) forms the DB. With a separate
    // training folder the exemplars come from there, so no test image is ever excluded.
    const std::vector<Item>& source = train.empty() ? items : train;
    std::vector<orec::Sample> oneShotDb, trainDb;
    std::map<std::string, size_t> exemplar;
    for (size_t i = 0; i < source.size(); ++i) {
        if (exemplar.emplace(source[i].label, i).second)
            oneShotDb.push_back({source[i].label, pick(source[i])});
        trainDb.push_back({source[i].label, pick(source[i])});
    }

    for (size_t i = 0; i < items.size(); ++i) {
        std::vector<orec::Sample> db;
        if (!train.empty()) {
            db = protocol == "split-one-shot" ? oneShotDb : trainDb;
        } else if (protocol == "loo") {
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
        if (match.label == items[i].label) correctDistances.push_back(match.distance);
        if (predicted != items[i].label)
            mistakes.push_back(items[i].file + "  (" + items[i].label + " -> " + predicted +
                               cv::format(", d=%.3f)", match.distance));
    }
    report(name, protocol, matrix, mistakes);
    if (!correctDistances.empty()) {
        std::sort(correctDistances.begin(), correctDistances.end());
        const auto at = [&](double q) {
            return correctDistances[static_cast<size_t>(q * (correctDistances.size() - 1))];
        };
        std::cout << cv::format("  correct-match distance: median %.4g, p95 %.4g, max %.4g\n",
                                at(0.5), at(0.95), correctDistances.back());
    }
}

}  // namespace

int main(int argc, char** argv) {
    cv::utils::logging::setLogLevel(cv::utils::logging::LOG_LEVEL_WARNING);

    std::string root = "data/objects", trainRoot, dbPath;
    std::string modelPath = "data/models/resnet18-v2-7.onnx";
    std::string protocol = "loo";
    double unknownThreshold = std::numeric_limits<double>::infinity();
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--db" && i + 1 < argc) {
            dbPath = argv[++i];
            protocol = "db";
        } else if (arg == "--train" && i + 1 < argc) {
            trainRoot = argv[++i];
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

    const auto load = [&](const std::string& dir) {
        std::vector<Item> out;
        FrameResult r;
        cv::Mat roi;
        for (const auto& img : listLabeledImages(dir)) {
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
            out.push_back(std::move(item));
        }
        return out;
    };
    const std::vector<Item> items = load(root);
    const std::vector<Item> train = trainRoot.empty() ? std::vector<Item>{} : load(trainRoot);
    if (!trainRoot.empty()) protocol = protocol == "one-shot" ? "split-one-shot" : "split";
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
    std::cout << "protocol: " << protocol << ", " << items.size() << " test images";
    if (!train.empty()) std::cout << ", " << train.size() << " training images";
    std::cout << "\n";

    run(
        "features", items, train, protocol, fixedDb,
        [](const Item& it) -> const std::vector<double>& { return it.features; },
        [&](const std::vector<orec::Sample>& db, const std::vector<double>& f) {
            return orec::classifyNearest(db, orec::featureStdevs(db), f, unknownThreshold);
        });

    // A fixed hand-feature DB has no embeddings, so that protocol compares features only.
    if (embedder && protocol != "db") {
        run(
            "embedding", items, train, protocol, {},
            [](const Item& it) -> const std::vector<double>& { return it.embedding; },
            [&](const std::vector<orec::Sample>& db, const std::vector<double>& e) {
                return orec::classifyCosine(db, e, unknownThreshold);
            });
        run(
            "embedding-ssd", items, train, protocol, {},
            [](const Item& it) -> const std::vector<double>& { return it.embedding; },
            [&](const std::vector<orec::Sample>& db, const std::vector<double>& e) {
                return orec::classifySsd(db, e, unknownThreshold);
            });
    }
    std::cout << "\nwrote out/eval/confusion_*.{md,csv}\n";
    return 0;
}
