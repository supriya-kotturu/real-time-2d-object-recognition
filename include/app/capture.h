#pragma once

#include <filesystem>
#include <opencv2/core.hpp>
#include <string>

// Millisecond resolution so two quick saves never overwrite each other (D-06).
std::string timestamp();

void saveImage(const std::filesystem::path& dir, const std::string& file, const cv::Mat& img);

#include "app/dataset.h"  // isValidLabel, kept with the other label/folder rules
