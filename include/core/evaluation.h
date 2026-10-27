#pragma once

#include <map>
#include <string>
#include <utility>
#include <vector>

namespace objectrecognition {

// T7: rows = true label, columns = predicted label. Columns are the row labels plus any
// extra prediction seen (e.g. "unknown"), so the matrix stays square for known classes.
class ConfusionMatrix {
   public:
    void add(const std::string& truth, const std::string& predicted);

    std::vector<std::string> rowLabels() const;
    std::vector<std::string> columnLabels() const;
    int count(const std::string& truth, const std::string& predicted) const;
    int total() const { return total_; }
    double accuracy() const;

    std::string toMarkdown() const;
    std::string toCsv() const;

   private:
    std::map<std::pair<std::string, std::string>, int> counts_;
    std::map<std::string, int> rowTotals_;
    std::vector<std::string> extraColumns_;
    int total_ = 0;
    int correct_ = 0;
};

}  // namespace objectrecognition
