#pragma once

#include <map>
#include <opencv2/core.hpp>
#include <string>
#include <vector>

#include "app/auto_learn.h"
#include "app/pipeline.h"
#include "core/classifier.h"
#include "core/evaluation.h"

// D-27: everything between "a frame arrived" and "here is the status", minus the transport:
// both DBs, the label, modes, classifier choice, auto-learn, live evaluation and overlays.
// server.exe drives it over HTTP; the browser build drives it from a Web Worker (WASM), so the
// GUI behaves the same against either, from one implementation.
//
// Embedding is two-phase because the browser's ResNet18 (onnxruntime-web) is asynchronous:
// process() says whether this frame needs an embedding and extracts the ROI; the caller embeds
// it however it can and hands the vector to finish().

struct Reply {
    int status = 200;
    std::string body;
};

std::string jsonEscape(const std::string& s);
std::string jsonError(const std::string& msg);

struct SessionConfig {
    std::string dbPath = "data/db/features.csv";
    std::string embedDbPath = "data/db/embeddings_resnet18.csv";  // per model (H-01)
    bool persist = true;  // write DB and live-eval changes to disk; false on public demos
    bool embeddingAvailable = false;
    std::string classifier = "features";
    std::string metric = "scaled-euclidean";
};

// Facts about the frame source that only the transport knows.
struct StatusExtras {
    std::string source;
    bool still = false;
    bool browser = false;
    bool isPublic = false;
    double fps = 0.0;
};

class Session {
   public:
    using Clock = AutoLearn::Clock;

    explicit Session(SessionConfig config);

    // --- Frame cycle: process(); if needsEmbedding(), embed roi(); finish(). ---
    void process(const cv::Mat& frameBgr, Clock::time_point now);
    bool needsEmbedding() const { return roiReady_; }
    const cv::Mat& roi() const { return roi_; }
    void finish(const std::vector<double>* embedding);  // nullptr when none was computed

    // For commands between frames that add a sample: the last frame's ROI, if it has one.
    bool currentRoi(cv::Mat& roi) const;

    // --- Outputs ---
    std::string statusJson(const StatusExtras& extras) const;
    const cv::Mat& frame() const { return frame_; }
    const FrameResult& result() const { return result_; }
    const cv::Mat& annotated() const { return display_; }
    const cv::Mat& regionsView() const { return regionsVis_; }
    const std::string& label() const { return label_; }
    bool embeddingAvailable() const { return config_.embeddingAvailable; }
    void setEmbeddingAvailable(bool available) { config_.embeddingAvailable = available; }

    // --- Commands: validated here, so every transport enforces the same rules. ---
    Reply db() const;
    Reply deleteLabel(const std::string& target);
    Reply resetDb();
    Reply setLabel(const std::string& next);
    Reply train(const std::vector<double>* embedding);
    Reply setCutoff(double value);
    Reply setClassifier(const std::string& classifier, const std::string& metric);
    Reply setMode(const std::string& next);
    Reply autoLearnLabel(const std::string& name);
    Reply autoLearnDismiss();
    Reply evalMatrix() const;
    Reply evalRecord();
    Reply evalReset();

   private:
    void addSample(const std::string& name, const std::vector<double>* embedding);
    std::string dbJson() const;
    std::string matrixJson() const;

    SessionConfig config_;
    std::string label_;
    std::string mode_ = "detect";
    std::vector<objectrecognition::Sample> db_, embedDb_;
    std::vector<double> stdevs_;
    // Each metric has its own scale, so each keeps its own unknown cutoff (D-25; defaults from
    // eval's correct-match distances).
    std::map<std::string, double> cutoffs_ = {
        {"scaled-euclidean", 3.0}, {"cosine", 0.35}, {"ssd", 500.0}};
    AutoLearn autoLearn_;
    objectrecognition::ConfusionMatrix liveMatrix_;

    cv::Mat frame_, display_, regionsVis_, roi_;
    FrameResult result_;
    Clock::time_point frameTime_{};
    bool roiReady_ = false;
    bool canClassify_ = false;
    objectrecognition::Match match_{};
    std::string currentPrediction_;  // this frame's label, "unknown", or "" with no object
};
