#include "app/frame_source.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <opencv2/imgcodecs.hpp>

#include "app/dataset.h"

namespace fs = std::filesystem;

FrameSource::FrameSource(const std::string& spec) {
    const bool isCameraIndex =
        !spec.empty() &&
        std::all_of(spec.begin(), spec.end(), [](unsigned char c) { return std::isdigit(c); });

    if (isCameraIndex) {
        cap_.open(std::stoi(spec));
        name_ = "camera" + spec;
    } else if (fs::is_directory(spec)) {
        for (const auto& entry : fs::directory_iterator(spec)) {
            if (entry.is_regular_file() && isImageFile(entry.path())) {
                images_.push_back(entry.path().string());
            }
        }
        std::sort(images_.begin(), images_.end());
    } else {
        cap_.open(spec);
        name_ = fs::path(spec).stem().string();
    }
}

bool FrameSource::isOpen() const { return cap_.isOpened() || !images_.empty(); }

bool FrameSource::next(cv::Mat& frame) {
    if (!images_.empty()) {
        // Skip unreadable files rather than ending the run on one bad image.
        while (idx_ < images_.size()) {
            const std::string& path = images_[idx_++];
            frame = cv::imread(path, cv::IMREAD_COLOR);
            if (!frame.empty()) {
                name_ = fs::path(path).stem().string();
                return true;
            }
        }
        return false;
    }
    return cap_.read(frame) && !frame.empty();
}
