#include "app/feature_db.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

std::vector<objectrecognition::Sample> loadFeatureDb(const std::string& path) {
    std::vector<objectrecognition::Sample> db;
    std::ifstream in(path);
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        std::stringstream ss(line);
        objectrecognition::Sample s;
        std::getline(ss, s.label, ',');
        std::string cell;
        while (std::getline(ss, cell, ',')) s.features.push_back(std::stod(cell));
        db.push_back(std::move(s));
    }
    return db;
}

void appendSample(const std::string& path, const objectrecognition::Sample& sample) {
    std::filesystem::create_directories(std::filesystem::path(path).parent_path());
    // Commas would split the label into a bogus extra column.
    std::string label = sample.label;
    std::replace(label.begin(), label.end(), ',', '_');

    std::ofstream out(path, std::ios::app);
    out << label << std::setprecision(10);
    for (double f : sample.features) out << ',' << f;
    out << '\n';
}
