#include "app/session.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <opencv2/imgproc.hpp>
#include <sstream>

#include "app/dataset.h"
#include "app/feature_db.h"
#include "app/overlay.h"
#include "app/roi.h"

namespace fs = std::filesystem;
namespace orec = objectrecognition;

std::string jsonEscape(const std::string& s) {
    std::string out;
    for (char ch : s) {
        switch (ch) {
            case '"':
                out += "\\\"";
                break;
            case '\\':
                out += "\\\\";
                break;
            case '\n':
                out += "\\n";
                break;
            default:
                if (static_cast<unsigned char>(ch) < 0x20) continue;
                out += ch;
        }
    }
    return out;
}

std::string jsonError(const std::string& msg) { return R"({"error":")" + jsonEscape(msg) + "\"}"; }

namespace {
const std::string kBadLabel = "label must be 1-64 chars of [A-Za-z0-9_-]";
}

Session::Session(SessionConfig config) : config_(std::move(config)) {
    db_ = loadFeatureDb(config_.dbPath);
    embedDb_ = loadFeatureDb(config_.embedDbPath);
    stdevs_ = orec::featureStdevs(db_);
    if (config_.classifier == "resnet18" && !config_.embeddingAvailable) {
        config_.classifier = "features";
        config_.metric = "scaled-euclidean";
    }
}

void Session::process(const cv::Mat& frameBgr, Clock::time_point now) {
    frame_ = frameBgr;
    frameTime_ = now;
    runPipeline(frame_, result_);
    // ResNet18 runs only when something will use it this frame, so features mode pays nothing
    // for it: the classifier itself, or auto-learn adding a sample to both DBs.
    const bool wanted =
        config_.classifier == "resnet18" ||
        (mode_ == "detect" && autoLearn_.state() == AutoLearn::State::Collecting);
    roiReady_ = config_.embeddingAvailable && wanted && currentRoi(roi_);
}

bool Session::currentRoi(cv::Mat& roi) const {
    if (!result_.hasFeatures || frame_.empty()) return false;
    return extractAlignedRoi(frame_, result_.regionMap, result_.regions[result_.primary].id,
                             result_.features, roi);
}

void Session::finish(const std::vector<double>* embedding) {
    frame_.copyTo(display_);
    colorizeRegions(result_.regionMap, regionsVis_);

    // Classify once per frame; status, auto-learn, and evaluation all reuse it.
    match_ = orec::Match{};
    const double cutoff = cutoffs_.at(config_.metric);
    if (config_.classifier == "resnet18") {
        canClassify_ = embedding && embedDb_.size() >= 2;
        if (canClassify_)
            match_ = config_.metric == "ssd" ? orec::classifySsd(embedDb_, *embedding, cutoff)
                                             : orec::classifyCosine(embedDb_, *embedding, cutoff);
    } else {
        canClassify_ = result_.hasFeatures && db_.size() >= 2;
        if (canClassify_)
            match_ = orec::classifyNearest(db_, stdevs_, result_.features.values, cutoff);
    }
    // With fewer than 2 samples nothing can be recognised, so every object counts as
    // unknown: that is what lets auto-learn bootstrap an empty DB.
    const bool unknown = result_.hasFeatures && (!canClassify_ || !match_.known);
    currentPrediction_ = !result_.hasFeatures ? "" : unknown ? "unknown" : match_.label;

    if (mode_ == "detect") {
        const double diagonal = std::hypot(frame_.cols, frame_.rows);
        autoLearn_.update(result_.hasFeatures, unknown, result_.features.centroid,
                          result_.features.theta, diagonal, frameTime_);
        if (result_.hasFeatures && autoLearn_.wantsSample(frameTime_)) {
            addSample(autoLearn_.label(), embedding);  // before sampleTaken: last pose clears it
            autoLearn_.sampleTaken(result_.features.centroid, result_.features.theta);
        }
    }

    if (result_.hasFeatures) {
        std::string text = canClassify_ ? currentPrediction_ : "object";
        if (autoLearn_.state() == AutoLearn::State::Tracking)
            text = cv::format("unknown %.1fs",
                              autoLearn_.holdProgress(frameTime_) * autoLearn_.holdSeconds());
        else if (autoLearn_.state() == AutoLearn::State::Prompt)
            text = "unknown: name me";
        else if (autoLearn_.state() == AutoLearn::State::Collecting)
            text = cv::format("%s %d/%d: %s", autoLearn_.label().c_str(), autoLearn_.collected(),
                              autoLearn_.targetSamples(),
                              autoLearn_.awaitingMove() ? "move or rotate" : "hold still");
        drawFeatures(display_, result_.features, text);
    }
}

void Session::addSample(const std::string& name, const std::vector<double>* embedding) {
    const orec::Sample sample{name, result_.features.values};
    if (config_.persist) appendSample(config_.dbPath, sample);
    db_.push_back(sample);
    stdevs_ = orec::featureStdevs(db_);
    // Both DBs grow together, so switching classifier never forgets what was just taught.
    if (embedding) {
        const orec::Sample embedded{name, *embedding};
        if (config_.persist) appendSample(config_.embedDbPath, embedded);
        embedDb_.push_back(embedded);
    }
}

std::string Session::statusJson(const StatusExtras& extras) const {
    std::ostringstream status;
    status << R"({"mode":")" << mode_ << R"(","threshold":)" << result_.threshold
           << R"(,"label":")" << jsonEscape(label_) << R"(","source":")"
           << jsonEscape(extras.source) << R"(","still":)" << (extras.still ? "true" : "false")
           << R"(,"browser":)" << (extras.browser ? "true" : "false") << R"(,"dbSize":)"
           << db_.size() << R"(,"public":)" << (extras.isPublic ? "true" : "false")
           << R"(,"embedDbSize":)" << embedDb_.size() << R"(,"classifier":")"
           << config_.classifier << R"(","metric":")" << config_.metric
           << R"(","resnetAvailable":)" << (config_.embeddingAvailable ? "true" : "false")
           << R"(,"unknownThreshold":)" << cutoffs_.at(config_.metric) << R"(,"fps":)"
           << cv::format("%.1f", extras.fps) << R"(,"autoLearn":{"state":")"
           << toString(autoLearn_.state()) << R"(","progress":)"
           << cv::format("%.2f", autoLearn_.holdProgress(frameTime_)) << R"(,"label":")"
           << jsonEscape(autoLearn_.label()) << R"(","collected":)" << autoLearn_.collected()
           << R"(,"target":)" << autoLearn_.targetSamples() << R"(,"phase":")"
           << (autoLearn_.awaitingMove() ? "move" : "hold") << R"("},"object":)";
    if (result_.hasFeatures) {
        status << R"({"features":{)";
        for (size_t i = 0; i < result_.features.values.size(); ++i)
            status << (i ? "," : "") << '"' << orec::kFeatureNames[i]
                   << "\":" << result_.features.values[i];
        status << R"(},"prediction":)";
        if (canClassify_) {
            status << R"({"label":")" << jsonEscape(match_.label) << R"(","distance":)"
                   << match_.distance << R"(,"known":)" << (match_.known ? "true" : "false")
                   << "}";
        } else {
            status << "null";
        }
        status << "}";
    } else {
        status << "null";
    }
    status << "}";
    return status.str();
}

std::string Session::dbJson() const {
    std::map<std::string, int> counts;
    for (const auto& s : db_) ++counts[s.label];
    std::ostringstream out;
    out << R"({"total":)" << db_.size() << R"(,"labels":[)";
    bool first = true;
    for (const auto& [name, count] : counts) {
        out << (first ? "" : ",") << R"({"label":")" << jsonEscape(name) << R"(","count":)"
            << count << "}";
        first = false;
    }
    out << "]}";
    return out.str();
}

std::string Session::matrixJson() const {
    const auto rows = liveMatrix_.rowLabels();
    const auto cols = liveMatrix_.columnLabels();
    std::ostringstream out;
    const auto list = [&](const std::vector<std::string>& v) {
        out << '[';
        for (size_t i = 0; i < v.size(); ++i)
            out << (i ? "," : "") << '"' << jsonEscape(v[i]) << '"';
        out << ']';
    };
    out << R"({"rows":)";
    list(rows);
    out << R"(,"columns":)";
    list(cols);
    out << R"(,"counts":[)";
    for (size_t r = 0; r < rows.size(); ++r) {
        out << (r ? "," : "") << '[';
        for (size_t c = 0; c < cols.size(); ++c)
            out << (c ? "," : "") << liveMatrix_.count(rows[r], cols[c]);
        out << ']';
    }
    out << R"(],"total":)" << liveMatrix_.total() << R"(,"accuracy":)" << liveMatrix_.accuracy()
        << '}';
    return out.str();
}

Reply Session::db() const { return {200, dbJson()}; }

Reply Session::deleteLabel(const std::string& target) {
    const auto drop = [&](std::vector<orec::Sample>& from) {
        const auto before = from.size();
        from.erase(std::remove_if(from.begin(), from.end(),
                                  [&](const orec::Sample& s) { return s.label == target; }),
                   from.end());
        return from.size() != before;
    };
    const bool droppedFeatures = drop(db_);
    const bool droppedEmbeddings = drop(embedDb_);
    if (!droppedFeatures && !droppedEmbeddings) return {404, jsonError("no such label")};
    if (config_.persist) {
        saveFeatureDb(config_.dbPath, db_);
        saveFeatureDb(config_.embedDbPath, embedDb_);
    }
    stdevs_ = orec::featureStdevs(db_);
    return {200, dbJson()};
}

// Undo every train/delete since the last save: on a public demo that is everything a visitor
// changed, since nothing there is written to disk.
Reply Session::resetDb() {
    db_ = loadFeatureDb(config_.dbPath);
    embedDb_ = loadFeatureDb(config_.embedDbPath);
    stdevs_ = orec::featureStdevs(db_);
    return {200, dbJson()};
}

Reply Session::setLabel(const std::string& next) {
    if (!isValidLabel(next)) return {400, jsonError(kBadLabel)};
    label_ = next;
    return {200, R"({"label":")" + label_ + "\"}"};
}

Reply Session::train(const std::vector<double>* embedding) {
    if (label_.empty()) return {409, jsonError("set a label first")};
    if (!result_.hasFeatures) return {409, jsonError("no object detected")};
    addSample(label_, embedding);
    return {200, dbJson()};
}

Reply Session::setCutoff(double value) {
    if (!(value > 0)) return {400, jsonError("unknownThreshold must be a positive number")};
    cutoffs_[config_.metric] = value;  // applies to the metric currently selected
    return {200, cv::format(R"({"unknownThreshold":%.3f})", value)};
}

Reply Session::setClassifier(const std::string& classifier, const std::string& metric) {
    if (classifier != "features" && classifier != "resnet18")
        return {400, jsonError("classifier must be features or resnet18")};
    const std::string next =
        !metric.empty() ? metric : classifier == "resnet18" ? "cosine" : "scaled-euclidean";
    const bool valid = classifier == "features" ? next == "scaled-euclidean"
                                                : next == "cosine" || next == "ssd";
    if (!valid)
        return {400, jsonError("features uses scaled-euclidean; resnet18 uses cosine or ssd")};
    if (classifier == "resnet18" && !config_.embeddingAvailable)
        return {409, jsonError("ResNet18 model is not loaded")};
    config_.classifier = classifier;
    config_.metric = next;
    // A prompt raised under the other classifier's notion of "unknown" is stale.
    autoLearn_ = AutoLearn{};
    return {200, R"({"classifier":")" + config_.classifier + R"(","metric":")" + config_.metric +
                     "\"}"};
}

Reply Session::setMode(const std::string& next) {
    if (next != "detect" && next != "collect" && next != "evaluate")
        return {400, jsonError("mode must be detect, collect, or evaluate")};
    mode_ = next;
    autoLearn_ = AutoLearn{};  // a half-finished prompt makes no sense in another mode
    return {200, R"({"mode":")" + mode_ + "\"}"};
}

Reply Session::autoLearnLabel(const std::string& name) {
    if (!isValidLabel(name)) return {400, jsonError(kBadLabel)};
    if (!autoLearn_.startCollecting(name))
        return {409, jsonError("no unknown object is waiting for a label")};
    return {200, R"({"collecting":")" + name + "\"}"};
}

Reply Session::autoLearnDismiss() {
    autoLearn_.dismiss();
    return {200, "{}"};
}

Reply Session::evalMatrix() const { return {200, matrixJson()}; }

// The task's evaluation hook: the user states the true label, the system tallies its guess.
Reply Session::evalRecord() {
    if (label_.empty()) return {409, jsonError("set the true label first")};
    if (currentPrediction_.empty()) return {409, jsonError("no object detected")};
    liveMatrix_.add(label_, currentPrediction_);
    if (config_.persist) {
        fs::create_directories("out/eval");
        std::ofstream("out/eval/live_confusion.md") << liveMatrix_.toMarkdown();
        std::ofstream("out/eval/live_confusion.csv") << liveMatrix_.toCsv();
    }
    return {200, matrixJson()};
}

Reply Session::evalReset() {
    liveMatrix_ = orec::ConfusionMatrix{};
    return {200, matrixJson()};
}
