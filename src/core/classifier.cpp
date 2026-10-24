#include "core/classifier.h"

#include <cmath>
#include <limits>
#include <stdexcept>

namespace objectrecognition {

std::vector<double> featureStdevs(const std::vector<Sample>& db) {
    if (db.empty()) return {};
    const size_t k = db.front().features.size();
    std::vector<double> mean(k, 0.0), var(k, 0.0);
    for (const auto& s : db)
        for (size_t i = 0; i < k; ++i) mean[i] += s.features[i];
    for (auto& m : mean) m /= static_cast<double>(db.size());
    for (const auto& s : db)
        for (size_t i = 0; i < k; ++i) var[i] += (s.features[i] - mean[i]) * (s.features[i] - mean[i]);

    std::vector<double> stdevs(k);
    for (size_t i = 0; i < k; ++i) stdevs[i] = std::sqrt(var[i] / static_cast<double>(db.size()));
    return stdevs;
}

double scaledEuclidean(const std::vector<double>& a, const std::vector<double>& b,
                       const std::vector<double>& stdevs) {
    double sum = 0.0;
    for (size_t i = 0; i < a.size(); ++i) {
        // A feature constant across the DB can't discriminate; skipping it avoids dividing
        // by ~0, which would let its noise dominate the distance.
        if (stdevs[i] < 1e-12) continue;
        const double d = (a[i] - b[i]) / stdevs[i];
        sum += d * d;
    }
    return std::sqrt(sum);
}

Match classifyNearest(const std::vector<Sample>& db, const std::vector<double>& stdevs,
                      const std::vector<double>& features, double unknownThreshold) {
    if (db.empty()) throw std::invalid_argument("classifyNearest: empty DB");
    Match best{"", std::numeric_limits<double>::max(), false};
    for (const auto& s : db) {
        if (s.features.size() != features.size()) continue;  // stale row from an older layout
        const double d = scaledEuclidean(features, s.features, stdevs);
        if (d < best.distance) best = {s.label, d, false};
    }
    best.known = best.distance <= unknownThreshold;
    return best;
}

}  // namespace objectrecognition
