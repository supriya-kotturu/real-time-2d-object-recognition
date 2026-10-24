#pragma once

#include <string>
#include <vector>

namespace objectrecognition {

struct Sample {
    std::string label;
    std::vector<double> features;
};

struct Match {
    std::string label;  // nearest DB label, even when !known
    double distance;
    bool known;  // false when distance exceeds the unknown-object threshold
};

// Per-feature standard deviation over the DB: the scale in scaled Euclidean distance, so a
// feature with a large numeric range can't dominate one with a small range.
std::vector<double> featureStdevs(const std::vector<Sample>& db);

// sqrt( sum_i ((a_i - b_i) / stdev_i)^2 )
double scaledEuclidean(const std::vector<double>& a, const std::vector<double>& b,
                       const std::vector<double>& stdevs);

// T6: nearest neighbour. Requires a non-empty DB.
Match classifyNearest(const std::vector<Sample>& db, const std::vector<double>& stdevs,
                      const std::vector<double>& features, double unknownThreshold);

}  // namespace objectrecognition
