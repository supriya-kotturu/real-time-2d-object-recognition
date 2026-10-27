// D-27: the browser build. A thin C API over Session (the same code server.exe runs), called
// from a Web Worker. JS owns the pixels and ResNet18 (onnxruntime-web); C++ owns tasks 1-4,
// the DBs, classification, auto-learn and the overlays.
//
// Per frame:  buf = orec_frame_buffer(w, h)  -> JS copies RGBA in
//             if (orec_process())            -> JS embeds orec_roi_tensor() with ResNet18
//             orec_finish(embedding | 0)
//             orec_view(0|1|2), orec_status(fps)
// Strings returned to JS stay valid until the next call that returns a string.

#include <emscripten/emscripten.h>

#include <memory>
#include <opencv2/imgproc.hpp>
#include <string>
#include <vector>

#include "app/roi.h"
#include "app/session.h"

namespace {

constexpr int kEmbeddingSize = 512;

std::unique_ptr<Session> session;
cv::Mat rgbaIn, bgr, view;
std::vector<float> tensor;
std::string text;
int lastStatus = 200;

const char* hold(std::string s) {
    text = std::move(s);
    return text.c_str();
}

std::vector<double> toDoubles(const float* e) {
    return std::vector<double>(e, e + kEmbeddingSize);
}

}  // namespace

extern "C" {

// The DB files must already be in the virtual filesystem (JS writes them with FS.writeFile).
// Nothing is persisted: each tab gets its own in-memory DB, reset on reload.
EMSCRIPTEN_KEEPALIVE int orec_init(int embeddingAvailable) {
    SessionConfig config;
    config.persist = false;
    config.embeddingAvailable = embeddingAvailable != 0;
    if (config.embeddingAvailable) {  // open on the method that recognises (D-26)
        config.classifier = "resnet18";
        config.metric = "cosine";
    }
    session = std::make_unique<Session>(config);
    return 1;
}

EMSCRIPTEN_KEEPALIVE void orec_set_embedding_available(int available) {
    session->setEmbeddingAvailable(available != 0);
}

EMSCRIPTEN_KEEPALIVE unsigned char* orec_frame_buffer(int width, int height) {
    rgbaIn.create(height, width, CV_8UC4);
    return rgbaIn.data;
}

// Returns 1 when this frame needs an embedding (see orec_roi_tensor).
EMSCRIPTEN_KEEPALIVE int orec_process() {
    cv::cvtColor(rgbaIn, bgr, cv::COLOR_RGBA2BGR);
    session->process(bgr, Session::Clock::now());
    return session->needsEmbedding() ? 1 : 0;
}

// 1x3x224x224 float input for ResNet18, prepared by the same code as server.exe's Embedder.
EMSCRIPTEN_KEEPALIVE float* orec_roi_tensor() {
    makeResnetInput(session->roi(), tensor);
    return tensor.data();
}

// For train between frames: this frame's ROI tensor, or null if it has none.
EMSCRIPTEN_KEEPALIVE float* orec_current_roi_tensor() {
    cv::Mat roi;
    if (!session->currentRoi(roi)) return nullptr;
    makeResnetInput(roi, tensor);
    return tensor.data();
}

EMSCRIPTEN_KEEPALIVE void orec_finish(const float* embedding) {
    if (!embedding) {
        session->finish(nullptr);
        return;
    }
    const auto e = toDoubles(embedding);
    session->finish(&e);
}

// 0 = annotated, 1 = threshold, 2 = regions; RGBA, same size as the input frame.
EMSCRIPTEN_KEEPALIVE unsigned char* orec_view(int which) {
    const cv::Mat& src = which == 1 ? session->result().binary
                         : which == 2 ? session->regionsView()
                                      : session->annotated();
    if (src.empty()) return nullptr;
    cv::cvtColor(src, view, src.channels() == 1 ? cv::COLOR_GRAY2RGBA : cv::COLOR_BGR2RGBA);
    return view.data;
}

EMSCRIPTEN_KEEPALIVE const char* orec_status(double fps) {
    return hold(session->statusJson({"browser", false, true, true, fps}));
}

// The GUI's API calls, by name. Reply body is returned; its HTTP-style status via
// orec_last_status(), so the worker can turn non-2xx into errors exactly as fetch did.
EMSCRIPTEN_KEEPALIVE const char* orec_command(const char* name, const char* a, const char* b,
                                              double number, const float* embedding) {
    const std::string cmd = name, arg1 = a ? a : "", arg2 = b ? b : "";
    Reply r;
    if (cmd == "db") r = session->db();
    else if (cmd == "deleteLabel") r = session->deleteLabel(arg1);
    else if (cmd == "resetDb") r = session->resetDb();
    else if (cmd == "setLabel") r = session->setLabel(arg1);
    else if (cmd == "train") {
        if (embedding) {
            const auto e = toDoubles(embedding);
            r = session->train(&e);
        } else {
            r = session->train(nullptr);
        }
    } else if (cmd == "setCutoff") r = session->setCutoff(number);
    else if (cmd == "setClassifier") r = session->setClassifier(arg1, arg2);
    else if (cmd == "setMode") r = session->setMode(arg1);
    else if (cmd == "autoLearnLabel") r = session->autoLearnLabel(arg1);
    else if (cmd == "autoLearnDismiss") r = session->autoLearnDismiss();
    else if (cmd == "evalMatrix") r = session->evalMatrix();
    else if (cmd == "evalRecord") r = session->evalRecord();
    else if (cmd == "evalReset") r = session->evalReset();
    else r = {404, jsonError("unknown command: " + cmd)};
    lastStatus = r.status;
    return hold(r.body);
}

EMSCRIPTEN_KEEPALIVE int orec_last_status() { return lastStatus; }

// --- D-28: samples survive a refresh. JS stores what was added (IndexedDB) and imports it back.

EMSCRIPTEN_KEEPALIVE int orec_added_count() { return session->samplesAdded(); }

EMSCRIPTEN_KEEPALIVE const char* orec_added_label() { return hold(session->lastAdded().label); }

EMSCRIPTEN_KEEPALIVE int orec_added_features_len() {
    return static_cast<int>(session->lastAdded().features.size());
}
EMSCRIPTEN_KEEPALIVE const double* orec_added_features() {
    return session->lastAdded().features.data();
}

// Null when the sample was added without an embedding (ResNet18 not loaded yet).
EMSCRIPTEN_KEEPALIVE const double* orec_added_embedding() {
    const auto& e = session->lastAdded().embedding;
    return e.size() == kEmbeddingSize ? e.data() : nullptr;
}

// The crop it learned from, scaled so its longer side is maxSide; RGBA. Null if none.
EMSCRIPTEN_KEEPALIVE unsigned char* orec_added_thumb(int maxSide) {
    const cv::Mat& roi = session->lastAdded().roi;
    if (roi.empty()) return nullptr;
    const double scale = static_cast<double>(maxSide) / std::max(roi.cols, roi.rows);
    cv::Mat small;
    cv::resize(roi, small, cv::Size(), std::min(1.0, scale), std::min(1.0, scale), cv::INTER_AREA);
    cv::cvtColor(small, view, cv::COLOR_BGR2RGBA);
    return view.data;
}
EMSCRIPTEN_KEEPALIVE int orec_thumb_width() { return view.cols; }
EMSCRIPTEN_KEEPALIVE int orec_thumb_height() { return view.rows; }

EMSCRIPTEN_KEEPALIVE void orec_import(const char* label, const double* features, int n,
                                      const float* embedding) {
    const std::vector<double> f(features, features + n);
    if (!embedding) {
        session->importSample(label, f, nullptr);
        return;
    }
    const auto e = toDoubles(embedding);
    session->importSample(label, f, &e);
}

}  // extern "C"
