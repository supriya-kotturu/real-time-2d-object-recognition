#include "core/evaluation.h"

#include <algorithm>
#include <iomanip>
#include <sstream>

namespace objectrecognition {

void ConfusionMatrix::add(const std::string& truth, const std::string& predicted) {
    ++counts_[{truth, predicted}];
    ++rowTotals_[truth];
    ++total_;
    if (truth == predicted) ++correct_;
}

std::vector<std::string> ConfusionMatrix::rowLabels() const {
    std::vector<std::string> rows;
    for (const auto& [label, n] : rowTotals_) rows.push_back(label);
    return rows;
}

std::vector<std::string> ConfusionMatrix::columnLabels() const {
    std::vector<std::string> cols = rowLabels();
    for (const auto& [key, n] : counts_) {
        if (std::find(cols.begin(), cols.end(), key.second) == cols.end())
            cols.push_back(key.second);
    }
    return cols;
}

int ConfusionMatrix::count(const std::string& truth, const std::string& predicted) const {
    const auto it = counts_.find({truth, predicted});
    return it == counts_.end() ? 0 : it->second;
}

double ConfusionMatrix::accuracy() const {
    return total_ == 0 ? 0.0 : static_cast<double>(correct_) / total_;
}

std::string ConfusionMatrix::toMarkdown() const {
    const auto rows = rowLabels();
    const auto cols = columnLabels();
    std::ostringstream out;
    out << "| true \\ predicted |";
    for (const auto& c : cols) out << ' ' << c << " |";
    out << " recall |\n|---|";
    for (size_t i = 0; i <= cols.size(); ++i) out << "---|";
    out << '\n';
    for (const auto& r : rows) {
        out << "| **" << r << "** |";
        for (const auto& c : cols) {
            const int n = count(r, c);
            out << ' ' << (r == c ? "**" + std::to_string(n) + "**" : std::to_string(n)) << " |";
        }
        out << ' ' << std::fixed << std::setprecision(2)
            << static_cast<double>(count(r, r)) / rowTotals_.at(r) << " |\n";
    }
    out << "\nAccuracy: " << correct_ << '/' << total_ << " = " << std::setprecision(3)
        << accuracy() << '\n';
    return out.str();
}

std::string ConfusionMatrix::toCsv() const {
    const auto rows = rowLabels();
    const auto cols = columnLabels();
    std::ostringstream out;
    out << "true\\predicted";
    for (const auto& c : cols) out << ',' << c;
    out << '\n';
    for (const auto& r : rows) {
        out << r;
        for (const auto& c : cols) out << ',' << count(r, c);
        out << '\n';
    }
    return out.str();
}

}  // namespace objectrecognition
