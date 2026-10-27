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

// 1 - cos(a, b), in [0, 2]. Scale-free, so an embedding's overall magnitude (driven by image
// contrast) doesn't affect the match; only its direction does. Task 9's suggested metric.
double cosineDistance(const std::vector<double>& a, const std::vector<double>& b);

// sum_i (a_i - b_i)^2, Task 9's other suggested metric. Unlike cosine it depends on the
// embedding's magnitude, so the same object under brighter light can land farther away.
double sumSquaredDifference(const std::vector<double>& a, const std::vector<double>& b);

// T9: one-shot nearest neighbour on embeddings by cosine distance. Requires a non-empty DB.
Match classifyCosine(const std::vector<Sample>& db, const std::vector<double>& features,
                     double unknownThreshold);

// T9 alternative (D-25): nearest neighbour on embeddings by SSD. Requires a non-empty DB.
Match classifySsd(const std::vector<Sample>& db, const std::vector<double>& features,
                  double unknownThreshold);

// T6: nearest neighbour. Requires a non-empty DB.
Match classifyNearest(const std::vector<Sample>& db, const std::vector<double>& stdevs,
                      const std::vector<double>& features, double unknownThreshold);

}  // namespace objectrecognition
