// Live viewer. Pipeline stages are wired in here as each task lands.
// Usage: live [camera-index | video-file | image-dir]   (default: camera 0)
// Keys: q/Esc quit
//       n     set the current object label (typed in the terminal; video pauses meanwhile)
//       s     save raw frame -> data/objects/<label>/<timestamp>.png
//             and each stage -> out/stages/<stage>/<label>/<timestamp>.png   (D-06)
//       any other key -> next still image

#include <chrono>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <opencv2/core/utils/logger.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <sstream>

#include "app/frame_source.h"
#include "core/threshold.h"

namespace fs = std::filesystem;
namespace orec = objectrecognition;

namespace {

// Millisecond resolution so two quick 's' presses never overwrite each other.
std::string timestamp() {
    using namespace std::chrono;
    const auto now = system_clock::now();
    const std::time_t secs = system_clock::to_time_t(now);
    const auto ms = duration_cast<milliseconds>(now.time_since_epoch()).count() % 1000;
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &secs);
#else
    localtime_r(&secs, &local);
#endif
    std::ostringstream out;
    out << std::put_time(&local, "%Y%m%d-%H%M%S") << '-' << std::setw(3) << std::setfill('0')
        << ms;
    return out.str();
}

void saveImage(const fs::path& dir, const std::string& file, const cv::Mat& img) {
    fs::create_directories(dir);
    cv::imwrite((dir / file).string(), img);
}

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

    // Declared outside the loop so the out-param stages reuse their buffers every frame.
    cv::Mat frame, gray, binary, display;
    while (source.next(frame)) {
        cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
        const int t = orec::isodataThreshold(gray);
        orec::applyThreshold(gray, t, binary);

        // Overlay goes on a deep copy so saved frames stay clean report/pipeline inputs.
        frame.copyTo(display);
        const std::string status = "t=" + std::to_string(t) + "  label=" +
                                   (label.empty() ? "<none: press n>" : label);
        cv::putText(display, status, {10, 30}, cv::FONT_HERSHEY_SIMPLEX, 0.7, {0, 0, 255}, 2);
        cv::imshow("original", display);
        cv::imshow("threshold", binary);

        const int key = cv::waitKey(source.isStill() ? 0 : 1);
        if (key == 'q' || key == 27) break;
        if (key == 'n') {
            // D-06 option (b): blocks the video loop while typing. If that proves unusable,
            // record why in decisions.md and fall back to a --label CLI arg (option a).
            std::cout << "label (click this terminal, type, Enter): " << std::flush;
            std::getline(std::cin, label);
        }
        if (key == 's') {
            if (label.empty()) {
                std::cout << "no label set: press n first\n";
                continue;
            }
            const std::string file = timestamp() + ".png";
            saveImage(rawRoot / label, file, frame);
            saveImage(stageRoot / "threshold" / label, file, binary);
            std::cout << "saved " << (rawRoot / label / file).string() << "\n";
        }
    }
    return 0;
}
