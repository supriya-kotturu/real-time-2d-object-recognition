#pragma once

#include <filesystem>
#include <opencv2/core.hpp>
#include <string>

// Millisecond resolution so two quick saves never overwrite each other (D-06).
std::string timestamp();

void saveImage(const std::filesystem::path& dir, const std::string& file, const cv::Mat& img);

// Labels become directory names (data/objects/<label>/) and CSV cells, so only
// [A-Za-z0-9_-] is accepted: blocks "../" path traversal from the web GUI and stray commas.
bool isValidLabel(const std::string& label);
