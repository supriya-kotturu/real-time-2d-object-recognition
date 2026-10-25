// Nearest-neighbour metrics (T6, T9, D-25), hand-computed on 2-D vectors.

#include <cmath>
#include <iostream>
#include <string>
#include <vector>

#include "core/classifier.h"

namespace orec = objectrecognition;

namespace {

int failures = 0;

void expect(bool ok, const std::string& what) {
    std::cout << (ok ? "[ OK ] " : "[FAIL] ") << what << "\n";
    if (!ok) ++failures;
}

bool near(double a, double b) { return std::abs(a - b) < 1e-9; }

}  // namespace

int main() {
    const std::vector<double> a{1, 2}, b{4, 6};
    expect(near(orec::sumSquaredDifference(a, b), 25.0), "SSD (1,2)-(4,6) = 3^2 + 4^2 = 25");
    expect(near(orec::cosineDistance({1, 0}, {0, 1}), 1.0), "cosine distance of orthogonal = 1");

    // Same direction, twice the magnitude: a brighter copy of the same embedding.
    const std::vector<double> dim{1, 1}, bright{2, 2};
    expect(near(orec::cosineDistance(dim, bright), 0.0), "cosine ignores magnitude");
    expect(near(orec::sumSquaredDifference(dim, bright), 2.0), "SSD does not");

    const std::vector<orec::Sample> db{{"left", {-1, 0}}, {"right", {1, 0}}};
    const auto m = orec::classifySsd(db, {0.8, 0.1}, 1.0);
    expect(m.label == "right" && near(m.distance, 0.05) && m.known,
           "classifySsd picks the nearest: (0.8,0.1)->right, d = 0.04 + 0.01");
    expect(!orec::classifySsd(db, {0.8, 0.1}, 0.01).known, "distance over the cutoff -> unknown");

    const std::vector<orec::Sample> mixed{{"stale", {0, 0, 0}}, {"right", {1, 0}}};
    expect(orec::classifySsd(mixed, {0, 0}, 10).label == "right",
           "rows with a different length (stale layout) are skipped");

    std::cout << (failures ? "FAILED: " : "all passed, failures: ") << failures << "\n";
    return failures ? 1 : 0;
}
