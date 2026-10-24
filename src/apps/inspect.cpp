// Headless batch run of tasks 1-4 over an image directory. Produces the report's required
// images and feature vectors without clicking through windows.
// Usage: inspect [image-dir]   (default: data/examples)
// Writes out/stages/{threshold,cleaned,regions,features,roi}/<stem>.png and out/stages/features.csv
// (roi = Task 9 steps A-C: the original image rotated so the primary axis lies along x).

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <opencv2/core/utils/logger.hpp>
#include <opencv2/imgcodecs.hpp>

#include "app/embedding.h"
#include "app/frame_source.h"
#include "app/overlay.h"
#include "app/pipeline.h"

namespace fs = std::filesystem;
namespace orec = objectrecognition;

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

    FrameResult result;
    cv::Mat frame, regionsVis, display, roi;
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
        if (extractAlignedRoi(frame, result.regionMap, result.regions[result.primary].id,
                              result.features, roi))
            cv::imwrite((out / "roi" / file).string(), roi);
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
