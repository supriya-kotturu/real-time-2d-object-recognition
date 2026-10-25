#include "app/dataset.h"

#include <algorithm>
#include <cctype>

namespace fs = std::filesystem;

bool isImageFile(const fs::path& p) {
    std::string ext = p.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp";
}

std::vector<LabeledImage> listLabeledImages(const fs::path& root) {
    std::vector<LabeledImage> out;
    if (!fs::is_directory(root)) return out;
    for (const auto& labelDir : fs::directory_iterator(root)) {
        if (!labelDir.is_directory()) continue;
        const std::string label = labelDir.path().filename().string();
        for (const auto& f : fs::directory_iterator(labelDir.path())) {
            if (f.is_regular_file() && isImageFile(f.path())) out.push_back({label, f.path()});
        }
    }
    std::sort(out.begin(), out.end(), [](const LabeledImage& a, const LabeledImage& b) {
        return a.label != b.label ? a.label < b.label : a.path < b.path;
    });
    return out;
}

bool isValidLabel(const std::string& label) {
    return !label.empty() && label.size() <= 64 &&
           std::all_of(label.begin(), label.end(),
                       [](unsigned char c) { return std::isalnum(c) || c == '_' || c == '-'; });
}
