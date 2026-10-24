#pragma once

#include <filesystem>
#include <string>
#include <vector>

struct LabeledImage {
    std::string label;
    std::filesystem::path path;
};

bool isImageFile(const std::filesystem::path& p);

// <root>/<label>/<image>: folder name = label (D-06). Sorted so runs are reproducible.
std::vector<LabeledImage> listLabeledImages(const std::filesystem::path& root);
