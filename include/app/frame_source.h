#pragma once

#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>
#include <string>
#include <vector>

// One iterator over every input mode task.md allows (camera, recorded video, image directory),
// so each executable handles all three identically.
class FrameSource {
   public:
    // "0", "1", ... -> camera index; existing directory -> its images, sorted; else -> video file.
    explicit FrameSource(const std::string& spec);

    bool isOpen() const;
    bool next(cv::Mat& frame);

    // Stills should block on a key press instead of streaming past.
    bool isStill() const { return !images_.empty(); }
    const std::string& currentName() const { return name_; }

   private:
    cv::VideoCapture cap_;
    std::vector<std::string> images_;
    size_t idx_ = 0;
    std::string name_;
};
