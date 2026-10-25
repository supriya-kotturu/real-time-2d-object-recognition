#include "core/classifier.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>

namespace objectrecognition {

std::vector<double> featureStdevs(const std::vector<Sample>& db) {
    if (db.empty()) return {};
    const size_t k = db.front().features.size();
    std::vector<double> mean(k, 0.0), var(k, 0.0);
    for (const auto& s : db)
        for (size_t i = 0; i < k; ++i) mean[i] += s.features[i];
    for (auto& m : mean) m /= static_cast<double>(db.size());
    for (const auto& s : db)
        for (size_t i = 0; i < k; ++i)
            var[i] += (s.features[i] - mean[i]) * (s.features[i] - mean[i]);

    // Floor at 1% of the feature's magnitude. A near-zero stdev (one class, or identical
    // samples) must not be skipped: skipping every feature makes every distance 0, so any
    // object matches. Flooring keeps the feature in play without letting noise explode it.
    std::vector<double> stdevs(k);
    for (size_t i = 0; i < k; ++i) {
        const double sd = std::sqrt(var[i] / static_cast<double>(db.size()));
        stdevs[i] = std::max({sd, 0.01 * std::abs(mean[i]), 1e-9});
    }
    return stdevs;
}

double scaledEuclidean(const std::vector<double>& a, const std::vector<double>& b,
                       const std::vector<double>& stdevs) {
    double sum = 0.0;
    for (size_t i = 0; i < a.size(); ++i) {
        const double d = (a[i] - b[i]) / stdevs[i];
        sum += d * d;
    }
    return std::sqrt(sum);
}

double cosineDistance(const std::vector<double>& a, const std::vector<double>& b) {
    double dot = 0, na = 0, nb = 0;
    for (size_t i = 0; i < a.size(); ++i) {
        dot += a[i] * b[i];
        na += a[i] * a[i];
        nb += b[i] * b[i];
    }
    if (na == 0 || nb == 0) return 1.0;  // a zero vector has no direction
    return 1.0 - dot / (std::sqrt(na) * std::sqrt(nb));
}

double sumSquaredDifference(const std::vector<double>& a, const std::vector<double>& b) {
    double sum = 0.0;
    for (size_t i = 0; i < a.size(); ++i) sum += (a[i] - b[i]) * (a[i] - b[i]);
    return sum;
}

namespace {

template <class Distance>
Match nearest(const std::vector<Sample>& db, const std::vector<double>& query,
              double unknownThreshold, const char* caller, Distance distance) {
    if (db.empty()) throw std::invalid_argument(std::string(caller) + ": empty DB");
    Match best{"", std::numeric_limits<double>::max(), false};
    for (const auto& s : db) {
        if (s.features.size() != query.size()) continue;  // stale row from an older layout
        const double d = distance(query, s.features);
        if (d < best.distance) best = {s.label, d, false};
    }
    best.known = best.distance <= unknownThreshold;
    return best;
}

}  // namespace

Match classifyNearest(const std::vector<Sample>& db, const std::vector<double>& stdevs,
                      const std::vector<double>& features, double unknownThreshold) {
    return nearest(db, features, unknownThreshold, "classifyNearest",
                   [&](const auto& a, const auto& b) { return scaledEuclidean(a, b, stdevs); });
}

Match classifyCosine(const std::vector<Sample>& db, const std::vector<double>& features,
                     double unknownThreshold) {
    return nearest(db, features, unknownThreshold, "classifyCosine",
                   [](const auto& a, const auto& b) { return cosineDistance(a, b); });
}

Match classifySsd(const std::vector<Sample>& db, const std::vector<double>& features,
                  double unknownThreshold) {
    return nearest(db, features, unknownThreshold, "classifySsd",
                   [](const auto& a, const auto& b) { return sumSquaredDifference(a, b); });
}

}  // namespace objectrecognition
