// HTTP backend for the web GUI (D-13). Runs the same pipeline as live, but instead of OpenCV
// windows it serves MJPEG streams and a small JSON API on 127.0.0.1.
// Usage: server [--browser | camera-index | video-file | stream-url | image-dir] [port]
//        (default: --browser 8080). --browser = frames pushed by the GUI's camera (D-24).
//
//   WS     /ws/frames                                  binary JPEG in, {"ok":true} ack out
//   GET    /stream?view=annotated|threshold|regions   multipart MJPEG
//   GET    /api/status                                 latest frame's threshold, features, match
//   POST   /api/db/reset                               reload both DBs from disk (D-26)
//   GET    /api/db                                     sample count per label
//   DELETE /api/db/<label>                             drop every sample with that label
//   POST   /api/label     {"label": "scissors"}
//   POST   /api/train                                  append current features under the label
//   POST   /api/capture                                save raw + stage images (D-06)
//   POST   /api/next                                   advance to the next still image
//   PUT    /api/settings  {"unknownThreshold": 3.0}   cutoff of the current metric
//   PUT    /api/mode      {"mode": "detect"|"collect"|"evaluate"}       (D-15)
//   PUT    /api/classifier {"classifier": "features"|"resnet18", "metric": "cosine"|"ssd"} (D-25)
//   POST   /api/autolearn/label  {"label": "mug"}    name the prompted unknown object
//   POST   /api/autolearn/dismiss                    ignore it until it leaves / stop collecting
//   GET    /api/eval                                 live confusion matrix
//   POST   /api/eval/record                          tally current prediction vs true label
//   DELETE /api/eval                                 reset the live matrix
//
// Threading: the pipeline, the DB, and the label are owned by the main thread. HTTP handlers
// never touch them; they queue a command and wait for its reply, so no locks around the DB.

// A 640x480 JPEG is ~50 KB; the default 16 MB cap only gives a hostile page room to make us
// buffer. Must precede the include: httplib reads it at compile time.
#define CPPHTTPLIB_WEBSOCKET_MAX_PAYLOAD_LENGTH (4 * 1024 * 1024)
#include <httplib.h>

#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <deque>
#include <fstream>
#include <functional>
#include <future>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <opencv2/core/utils/logger.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <regex>
#include <sstream>
#include <thread>

#include "app/auto_learn.h"
#include "app/embedding.h"
#include "app/capture.h"
#include "app/feature_db.h"
#include "app/frame_mailbox.h"
#include "app/frame_source.h"
#include "app/overlay.h"
#include "app/pipeline.h"
#include "core/classifier.h"
#include "core/evaluation.h"

namespace fs = std::filesystem;
namespace orec = objectrecognition;
using namespace std::chrono_literals;

namespace {

const std::string kDbPath = "data/db/features.csv";
const std::string kEmbedDbPath = "data/db/embeddings_resnet18.csv";  // per model (H-01)
const std::string kModelPath = "data/models/resnet18-v2-7.onnx";
const std::vector<std::string> kViews = {"annotated", "threshold", "regions"};
// Morphology n and the min-area filter are in pixels, tuned on 640x480 captures (D-07, D-21),
// so larger browser frames are scaled down to this width before the pipeline sees them.
constexpr int kPipelineWidth = 640;
constexpr int kMaxFrameSide = 4096;

struct Reply {
    int status = 200;
    std::string body;
};

// Everything the HTTP threads may read; guarded by m.
struct Shared {
    std::mutex m;
    std::condition_variable frameReady;
    std::uint64_t frameId = 0;
    std::map<std::string, std::vector<uchar>> jpeg;
    std::string statusJson = "{}";
    std::deque<std::pair<std::function<Reply()>, std::promise<Reply>>> commands;
    std::atomic<bool> running{true};
};

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

// The GUI only ever sends flat objects, so a full JSON parser isn't worth a dependency.
bool extractString(const std::string& body, const std::string& key, std::string& out) {
    std::smatch m;
    if (!std::regex_search(body, m, std::regex("\"" + key + "\"\\s*:\\s*\"([^\"]*)\"")))
        return false;
    out = m[1];
    return true;
}

bool extractNumber(const std::string& body, const std::string& key, double& out) {
    std::smatch m;
    if (!std::regex_search(body, m, std::regex("\"" + key + "\"\\s*:\\s*(-?[0-9]+(?:\\.[0-9]+)?)")))
        return false;
    out = std::stod(m[1]);
    return true;
}

// Queues work for the pipeline thread and blocks the HTTP thread until it's done.
Reply runOnPipeline(Shared& shared, std::function<Reply()> fn) {
    std::promise<Reply> promise;
    auto future = promise.get_future();
    {
        std::lock_guard lock(shared.m);
        shared.commands.emplace_back(std::move(fn), std::move(promise));
    }
    if (future.wait_for(3s) != std::future_status::ready) return {503, jsonError("pipeline busy")};
    return future.get();
}

void sendJson(httplib::Response& res, const Reply& reply) {
    res.status = reply.status;
    res.set_content(reply.body, "application/json");
}

// Loopback binding doesn't stop other websites: a page on evil.example can still make the
// user's browser POST to 127.0.0.1 (as a CORS "simple" request) or open a WebSocket, which
// CORS doesn't cover at all. Browsers always send Origin on those, so only the GUI's own
// origin is let through; requests without Origin (curl, <img> stream loads) are not cross-site.
// Same-origin also passes: in --public mode this server serves the GUI itself (D-26), so the
// page's Origin host equals the Host it sends, behind any TLS-terminating proxy.
bool isAllowedOrigin(const httplib::Request& req) {
    if (!req.has_header("Origin")) return true;
    const std::string origin = req.get_header_value("Origin");
    static const std::regex local(R"(^http://(localhost|127\.0\.0\.1)(:[0-9]{1,5})?$)");
    if (std::regex_match(origin, local)) return true;
    static const std::regex scheme(R"(^https?://)");
    return req.has_header("Host") &&
           std::regex_replace(origin, scheme, "") == req.get_header_value("Host");
}

}  // namespace

int main(int argc, char** argv) {
    cv::utils::logging::setLogLevel(cv::utils::logging::LOG_LEVEL_WARNING);

    // --public (D-26): hosted demo. Listens on every interface, keeps visitors' DB changes in
    // memory only (the committed DB is never modified), disables Capture (it writes images),
    // and serves the static GUI from web/out so page, API and WebSocket share one origin.
    bool isPublic = false;
    std::vector<std::string> positional;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--public")
            isPublic = true;
        else
            positional.push_back(arg);
    }
    const std::string spec = positional.size() > 0 ? positional[0] : "--browser";
    const int port = positional.size() > 1 ? std::stoi(positional[1]) : 8080;
    const std::string host = isPublic ? "0.0.0.0" : "127.0.0.1";
    const bool persist = !isPublic;
    const bool browser = spec == "--browser";
    std::optional<FrameSource> source;
    if (!browser) {
        source.emplace(spec);
        if (!source->isOpen()) {
            std::cerr << "cannot open input: " << spec << "\n";
            return 1;
        }
    }
    const auto isStill = [&] { return source && source->isStill(); };
    const auto sourceName = [&] { return source ? source->currentName() : std::string("browser"); };

    Shared shared;
    FrameMailbox mailbox;
    std::atomic<int> cameraClients{0};
    httplib::Server http;
    if (fs::is_directory("web/out")) http.set_mount_point("/", "web/out");

    // Runs before routing and before WebSocket upgrades.
    http.set_pre_routing_handler([](const httplib::Request& req, httplib::Response& res) {
        if (isAllowedOrigin(req)) return httplib::Server::HandlerResponse::Unhandled;
        sendJson(res, {403, jsonError("origin not allowed")});
        return httplib::Server::HandlerResponse::Handled;
    });

    // One connection per camera tab. The ack is sent only after the pipeline took the frame,
    // so a client that waits for it never has more than one frame in flight (H-50).
    http.WebSocket("/ws/frames", [&](const httplib::Request&, httplib::ws::WebSocket& ws) {
        // One camera at a time: two would interleave frames from different scenes, and every
        // visitor shares one pipeline, so a second tab would hijack the first one's view.
        if (cameraClients.fetch_add(1) > 0) {
            --cameraClients;
            ws.send(jsonError("camera busy: another tab or visitor is streaming"));
            ws.close();
            return;
        }
        struct Release {
            std::atomic<int>& n;
            ~Release() { --n; }
        } release{cameraClients};
        ws.set_read_timeout(1s);  // wake up regularly so an idle tab can't block shutdown
        std::string msg;
        while (shared.running && ws.is_open()) {
            const auto kind = ws.read(msg);
            if (kind == httplib::ws::Timeout) continue;
            if (kind == httplib::ws::Fail) break;
            if (kind != httplib::ws::Binary) {
                ws.send(jsonError("send JPEG frames as binary messages"));
                continue;
            }
            if (!browser) {
                ws.send(jsonError("server was started with a fixed source"));
                continue;
            }
            const cv::Mat bytes(1, static_cast<int>(msg.size()), CV_8UC1, msg.data());
            cv::Mat decoded = cv::imdecode(bytes, cv::IMREAD_COLOR);
            if (decoded.empty() || decoded.cols > kMaxFrameSide || decoded.rows > kMaxFrameSide) {
                ws.send(jsonError("not a decodable JPEG"));
                continue;
            }
            if (decoded.cols > kPipelineWidth) {
                const double scale = static_cast<double>(kPipelineWidth) / decoded.cols;
                cv::resize(decoded, decoded, cv::Size(), scale, scale, cv::INTER_AREA);
            }
            const auto seq = mailbox.put(std::move(decoded));
            if (!mailbox.waitConsumed(seq, 3s)) break;  // shutting down, or the pipeline hung
            ws.send(R"({"ok":true})");
        }
    });

    http.Get("/stream", [&](const httplib::Request& req, httplib::Response& res) {
        const std::string view = req.has_param("view") ? req.get_param_value("view") : "annotated";
        if (std::find(kViews.begin(), kViews.end(), view) == kViews.end()) {
            sendJson(res, {400, jsonError("unknown view")});
            return;
        }
        res.set_header("Cache-Control", "no-store");
        res.set_chunked_content_provider(
            "multipart/x-mixed-replace; boundary=frame",
            [&shared, view, last = std::uint64_t{0}](size_t, httplib::DataSink& sink) mutable {
                std::vector<uchar> buf;
                {
                    std::unique_lock lock(shared.m);
                    shared.frameReady.wait_for(
                        lock, 1s, [&] { return shared.frameId != last || !shared.running; });
                    if (!shared.running) return false;
                    if (shared.frameId == last) return true;  // no new frame yet; keep open
                    last = shared.frameId;
                    buf = shared.jpeg[view];
                }
                const std::string head = "--frame\r\nContent-Type: image/jpeg\r\nContent-Length: " +
                                         std::to_string(buf.size()) + "\r\n\r\n";
                return sink.write(head.data(), head.size()) &&
                       sink.write(reinterpret_cast<const char*>(buf.data()), buf.size()) &&
                       sink.write("\r\n", 2);
            });
    });

    http.Get("/api/status", [&](const httplib::Request&, httplib::Response& res) {
        std::lock_guard lock(shared.m);
        sendJson(res, {200, shared.statusJson});
    });

    // --- Pipeline-thread state; only touched inside runOnPipeline commands or the loop. ---
    std::string label;
    std::vector<orec::Sample> db = loadFeatureDb(kDbPath);
    std::vector<double> stdevs = orec::featureStdevs(db);
    std::vector<orec::Sample> embedDb = loadFeatureDb(kEmbedDbPath);
    FrameResult result;
    cv::Mat frame, display, regionsVis;
    bool advance = true;
    std::string mode = "detect";
    AutoLearn autoLearn;
    orec::ConfusionMatrix liveMatrix;
    std::string currentPrediction;  // this frame's label, "unknown", or "" with no object

    // D-25: classifier + metric chosen in the GUI. Each metric has its own scale, so each keeps
    // its own unknown cutoff (defaults from eval's correct-match distances).
    std::string classifier = "features";
    std::string metric = "scaled-euclidean";
    std::map<std::string, double> cutoffs = {
        {"scaled-euclidean", 3.0}, {"cosine", 0.35}, {"ssd", 500.0}};

    std::unique_ptr<Embedder> embedder;
    if (fs::exists(kModelPath)) {
        try {
            embedder = std::make_unique<Embedder>(kModelPath);
        } catch (const cv::Exception& e) {
            std::cerr << "ResNet18 unavailable: " << e.what() << "\n";
        }
    }

    // The current frame's embedding, computed at most once per frame and only when something
    // asks for it, so features mode pays nothing for ResNet18.
    std::vector<double> embedding;
    bool embeddingFresh = false;
    const auto currentEmbedding = [&]() -> const std::vector<double>* {
        if (!embedder || !result.hasFeatures) return nullptr;
        if (!embeddingFresh) {
            cv::Mat roi;
            if (!extractAlignedRoi(frame, result.regionMap, result.regions[result.primary].id,
                                   result.features, roi))
                return nullptr;
            embedder->embed(roi, embedding);
            embeddingFresh = true;
        }
        return &embedding;
    };

    const auto addSample = [&](const std::string& name) {
        const orec::Sample sample{name, result.features.values};
        if (persist) appendSample(kDbPath, sample);
        db.push_back(sample);
        stdevs = orec::featureStdevs(db);
        // Both DBs grow together, so switching classifier never forgets what was just taught.
        if (const auto* e = currentEmbedding()) {
            const orec::Sample embedded{name, *e};
            if (persist) appendSample(kEmbedDbPath, embedded);
            embedDb.push_back(embedded);
        }
    };

    const auto matrixJson = [&] {
        const auto rows = liveMatrix.rowLabels();
        const auto cols = liveMatrix.columnLabels();
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
                out << (c ? "," : "") << liveMatrix.count(rows[r], cols[c]);
            out << ']';
        }
        out << R"(],"total":)" << liveMatrix.total() << R"(,"accuracy":)" << liveMatrix.accuracy()
            << '}';
        return out.str();
    };

    const auto dbJson = [&] {
        std::map<std::string, int> counts;
        for (const auto& s : db) ++counts[s.label];
        std::ostringstream out;
        out << R"({"total":)" << db.size() << R"(,"labels":[)";
        bool first = true;
        for (const auto& [name, count] : counts) {
            out << (first ? "" : ",") << R"({"label":")" << jsonEscape(name) << R"(","count":)"
                << count << "}";
            first = false;
        }
        out << "]}";
        return out.str();
    };

    http.Get("/api/db", [&](const httplib::Request&, httplib::Response& res) {
        sendJson(res, runOnPipeline(shared, [&] { return Reply{200, dbJson()}; }));
    });

    // Undo every train/delete since the last save: on the public demo that is everything a
    // visitor changed, since nothing there is written to disk.
    http.Post("/api/db/reset", [&](const httplib::Request&, httplib::Response& res) {
        sendJson(res, runOnPipeline(shared, [&] {
                     db = loadFeatureDb(kDbPath);
                     embedDb = loadFeatureDb(kEmbedDbPath);
                     stdevs = orec::featureStdevs(db);
                     return Reply{200, dbJson()};
                 }));
    });

    http.Delete(
        R"(/api/db/([A-Za-z0-9_-]+))", [&](const httplib::Request& req, httplib::Response& res) {
            const std::string target = req.matches[1];
            sendJson(res, runOnPipeline(shared, [&, target] {
                         const auto drop = [&](std::vector<orec::Sample>& from) {
                             const auto before = from.size();
                             from.erase(std::remove_if(from.begin(), from.end(),
                                                       [&](const orec::Sample& s) {
                                                           return s.label == target;
                                                       }),
                                        from.end());
                             return from.size() != before;
                         };
                         const bool droppedFeatures = drop(db);
                         const bool droppedEmbeddings = drop(embedDb);
                         if (!droppedFeatures && !droppedEmbeddings)
                             return Reply{404, jsonError("no such label")};
                         if (persist) {
                             saveFeatureDb(kDbPath, db);
                             saveFeatureDb(kEmbedDbPath, embedDb);
                         }
                         stdevs = orec::featureStdevs(db);
                         return Reply{200, dbJson()};
                     }));
        });

    http.Post("/api/label", [&](const httplib::Request& req, httplib::Response& res) {
        std::string next;
        if (!extractString(req.body, "label", next) || !isValidLabel(next)) {
            sendJson(res, {400, jsonError("label must be 1-64 chars of [A-Za-z0-9_-]")});
            return;
        }
        sendJson(res, runOnPipeline(shared, [&, next] {
                     label = next;
                     return Reply{200, R"({"label":")" + label + "\"}"};
                 }));
    });

    http.Post("/api/train", [&](const httplib::Request&, httplib::Response& res) {
        sendJson(res, runOnPipeline(shared, [&] {
                     if (label.empty()) return Reply{409, jsonError("set a label first")};
                     if (!result.hasFeatures) return Reply{409, jsonError("no object detected")};
                     addSample(label);
                     return Reply{200, dbJson()};
                 }));
    });

    http.Post("/api/capture", [&](const httplib::Request&, httplib::Response& res) {
        sendJson(res, runOnPipeline(shared, [&] {
                     if (label.empty()) return Reply{409, jsonError("set a label first")};
                     if (!persist)
                         return Reply{403, jsonError("capture is disabled on the public demo")};
                     if (frame.empty()) return Reply{409, jsonError("no camera frame yet")};
                     const std::string file = timestamp() + ".png";
                     saveImage(fs::path("data/objects") / label, file, frame);
                     saveImage(fs::path("out/stages/threshold") / label, file, result.binary);
                     saveImage(fs::path("out/stages/regions") / label, file, regionsVis);
                     saveImage(fs::path("out/stages/features") / label, file, display);
                     return Reply{200, R"({"saved":"data/objects/)" + label + "/" + file + "\"}"};
                 }));
    });

    http.Post("/api/next", [&](const httplib::Request&, httplib::Response& res) {
        sendJson(res, runOnPipeline(shared, [&] {
                     advance = true;
                     return Reply{200, "{}"};
                 }));
    });

    http.Put("/api/settings", [&](const httplib::Request& req, httplib::Response& res) {
        double value = 0;
        if (!extractNumber(req.body, "unknownThreshold", value) || value <= 0) {
            sendJson(res, {400, jsonError("unknownThreshold must be a positive number")});
            return;
        }
        sendJson(res, runOnPipeline(shared, [&, value] {
                     cutoffs[metric] = value;  // applies to the metric currently selected
                     return Reply{200, cv::format(R"({"unknownThreshold":%.3f})", value)};
                 }));
    });

    http.Put("/api/classifier", [&](const httplib::Request& req, httplib::Response& res) {
        std::string nextClassifier, nextMetric;
        if (!extractString(req.body, "classifier", nextClassifier) ||
            (nextClassifier != "features" && nextClassifier != "resnet18")) {
            sendJson(res, {400, jsonError("classifier must be features or resnet18")});
            return;
        }
        const bool hasMetric = extractString(req.body, "metric", nextMetric);
        if (!hasMetric) nextMetric = nextClassifier == "resnet18" ? "cosine" : "scaled-euclidean";
        const bool valid = nextClassifier == "features"
                               ? nextMetric == "scaled-euclidean"
                               : nextMetric == "cosine" || nextMetric == "ssd";
        if (!valid) {
            sendJson(res, {400, jsonError("features uses scaled-euclidean; resnet18 uses cosine "
                                          "or ssd")});
            return;
        }
        sendJson(res, runOnPipeline(shared, [&, nextClassifier, nextMetric] {
                     if (nextClassifier == "resnet18" && !embedder)
                         return Reply{409, jsonError("ResNet18 model not found at " + kModelPath)};
                     classifier = nextClassifier;
                     metric = nextMetric;
                     // A prompt raised under the other classifier's notion of "unknown" is stale.
                     autoLearn = AutoLearn{};
                     return Reply{200, R"({"classifier":")" + classifier + R"(","metric":")" +
                                           metric + "\"}"};
                 }));
    });

    http.Put("/api/mode", [&](const httplib::Request& req, httplib::Response& res) {
        std::string next;
        if (!extractString(req.body, "mode", next) ||
            (next != "detect" && next != "collect" && next != "evaluate")) {
            sendJson(res, {400, jsonError("mode must be detect, collect, or evaluate")});
            return;
        }
        sendJson(res, runOnPipeline(shared, [&, next] {
                     mode = next;
                     autoLearn =
                         AutoLearn{};  // a half-finished prompt makes no sense in another mode
                     return Reply{200, R"({"mode":")" + mode + "\"}"};
                 }));
    });

    http.Post("/api/autolearn/label", [&](const httplib::Request& req, httplib::Response& res) {
        std::string name;
        if (!extractString(req.body, "label", name) || !isValidLabel(name)) {
            sendJson(res, {400, jsonError("label must be 1-64 chars of [A-Za-z0-9_-]")});
            return;
        }
        sendJson(res, runOnPipeline(shared, [&, name] {
                     if (!autoLearn.startCollecting(name))
                         return Reply{409, jsonError("no unknown object is waiting for a label")};
                     return Reply{200, R"({"collecting":")" + name + "\"}"};
                 }));
    });

    http.Post("/api/autolearn/dismiss", [&](const httplib::Request&, httplib::Response& res) {
        sendJson(res, runOnPipeline(shared, [&] {
                     autoLearn.dismiss();
                     return Reply{200, "{}"};
                 }));
    });

    http.Get("/api/eval", [&](const httplib::Request&, httplib::Response& res) {
        sendJson(res, runOnPipeline(shared, [&] { return Reply{200, matrixJson()}; }));
    });

    // The task's evaluation hook: the user states the true label, the system tallies its guess.
    http.Post("/api/eval/record", [&](const httplib::Request&, httplib::Response& res) {
        sendJson(res, runOnPipeline(shared, [&] {
                     if (label.empty()) return Reply{409, jsonError("set the true label first")};
                     if (currentPrediction.empty())
                         return Reply{409, jsonError("no object detected")};
                     liveMatrix.add(label, currentPrediction);
                     if (persist) {
                         fs::create_directories("out/eval");
                         std::ofstream("out/eval/live_confusion.md") << liveMatrix.toMarkdown();
                         std::ofstream("out/eval/live_confusion.csv") << liveMatrix.toCsv();
                     }
                     return Reply{200, matrixJson()};
                 }));
    });

    http.Delete("/api/eval", [&](const httplib::Request&, httplib::Response& res) {
        sendJson(res, runOnPipeline(shared, [&] {
                     liveMatrix = orec::ConfusionMatrix{};
                     return Reply{200, matrixJson()};
                 }));
    });

    // Loopback unless --public: locally the API writes files and deletes DB rows, so it must
    // not face the LAN. The public mode writes nothing to disk.
    std::thread httpThread([&] {
        if (!http.listen(host, port)) {
            std::cerr << "cannot listen on " << host << ":" << port << "\n";
            shared.running = false;
        }
    });
    std::cout << "serving on http://" << host << ":" << port << (isPublic ? " [public]" : "")
              << "  (source: " << spec
              << ", db: " << db.size() << " samples)" << std::endl;
    if (browser)
        std::cout << "waiting for the GUI camera on ws://127.0.0.1:" << port << "/ws/frames"
                  << std::endl;

    const std::vector<int> jpegParams = {cv::IMWRITE_JPEG_QUALITY, 80};
    double fps = 0.0;
    auto lastTick = std::chrono::steady_clock::now();

    const auto runCommands = [&] {
        std::deque<std::pair<std::function<Reply()>, std::promise<Reply>>> pending;
        {
            std::lock_guard lock(shared.m);
            pending.swap(shared.commands);
        }
        for (auto& [fn, promise] : pending) promise.set_value(fn());
    };

    // The last processed frame's classification; outlives the frame so idle ticks can still
    // report it.
    bool canClassify = false;
    orec::Match match{};
    auto frameTime = AutoLearn::Clock::now();

    const auto statusJson = [&] {
        std::ostringstream status;
        status << R"({"mode":")" << mode << R"(","threshold":)" << result.threshold
               << R"(,"label":")" << jsonEscape(label) << R"(","source":")"
               << jsonEscape(sourceName()) << R"(","still":)" << (isStill() ? "true" : "false")
               << R"(,"browser":)" << (browser ? "true" : "false") << R"(,"dbSize":)" << db.size()
               << R"(,"public":)" << (isPublic ? "true" : "false")
               << R"(,"embedDbSize":)" << embedDb.size() << R"(,"classifier":")" << classifier
               << R"(","metric":")" << metric << R"(","resnetAvailable":)"
               << (embedder ? "true" : "false") << R"(,"unknownThreshold":)" << cutoffs[metric]
               << R"(,"fps":)"
               << cv::format("%.1f", fps) << R"(,"autoLearn":{"state":")"
               << toString(autoLearn.state()) << R"(","progress":)"
               << cv::format("%.2f", autoLearn.holdProgress(frameTime)) << R"(,"label":")"
               << jsonEscape(autoLearn.label()) << R"(","collected":)" << autoLearn.collected()
               << R"(,"target":)" << autoLearn.targetSamples() << R"(,"phase":")"
               << (autoLearn.awaitingMove() ? "move" : "hold") << R"("},"object":)";
        if (result.hasFeatures) {
            status << R"({"features":{)";
            for (size_t i = 0; i < result.features.values.size(); ++i)
                status << (i ? "," : "") << '"' << orec::kFeatureNames[i]
                       << "\":" << result.features.values[i];
            status << R"(},"prediction":)";
            if (canClassify) {
                status << R"({"label":")" << jsonEscape(match.label) << R"(","distance":)"
                       << match.distance << R"(,"known":)" << (match.known ? "true" : "false")
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
    };

    while (shared.running) {
        if (browser) {
            // No new frame = camera stopped or not started. Keep answering the API, but don't
            // re-run the pipeline on a frozen frame: auto-learn would read it as "held still".
            cv::Mat next;
            if (!mailbox.take(next, 100ms)) {
                runCommands();
                const std::string status = statusJson();  // mode/label changes show while idle
                std::lock_guard lock(shared.m);
                shared.statusJson = status;
                continue;
            }
            frame = next;
        } else if (!source->isStill() || advance) {
            // Stills are re-processed each tick so settings changes show up without a new frame.
            cv::Mat next;
            if (!source->next(next)) {
                if (!source->isStill()) break;  // video ended / camera lost
            } else {
                frame = next;
            }
            advance = false;
        }
        if (frame.empty()) break;

        runPipeline(frame, result);
        embeddingFresh = false;
        frame.copyTo(display);
        colorizeRegions(result.regionMap, regionsVis);
        frameTime = AutoLearn::Clock::now();

        // Classify once per frame; status, auto-learn, and evaluation all reuse it.
        match = orec::Match{};
        const double cutoff = cutoffs[metric];
        if (classifier == "resnet18") {
            const auto* e = currentEmbedding();
            canClassify = e && embedDb.size() >= 2;
            if (canClassify)
                match = metric == "ssd" ? orec::classifySsd(embedDb, *e, cutoff)
                                        : orec::classifyCosine(embedDb, *e, cutoff);
        } else {
            canClassify = result.hasFeatures && db.size() >= 2;
            if (canClassify)
                match = orec::classifyNearest(db, stdevs, result.features.values, cutoff);
        }
        // With fewer than 2 samples nothing can be recognised, so every object counts as
        // unknown: that is what lets auto-learn bootstrap an empty DB.
        const bool unknown = result.hasFeatures && (!canClassify || !match.known);
        currentPrediction = !result.hasFeatures ? "" : unknown ? "unknown" : match.label;

        if (mode == "detect") {
            const double diagonal = std::hypot(frame.cols, frame.rows);
            autoLearn.update(result.hasFeatures, unknown, result.features.centroid,
                             result.features.theta, diagonal, frameTime);
            if (result.hasFeatures && autoLearn.wantsSample(frameTime)) {
                addSample(autoLearn.label());  // before sampleTaken: the last pose clears it
                autoLearn.sampleTaken(result.features.centroid, result.features.theta);
            }
        }

        if (result.hasFeatures) {
            std::string text = canClassify ? currentPrediction : "object";
            if (autoLearn.state() == AutoLearn::State::Tracking)
                text = cv::format("unknown %.1fs",
                                  autoLearn.holdProgress(frameTime) * autoLearn.holdSeconds());
            else if (autoLearn.state() == AutoLearn::State::Prompt)
                text = "unknown: name me";
            else if (autoLearn.state() == AutoLearn::State::Collecting)
                text = cv::format("%s %d/%d: %s", autoLearn.label().c_str(), autoLearn.collected(),
                                  autoLearn.targetSamples(),
                                  autoLearn.awaitingMove() ? "move or rotate" : "hold still");
            drawFeatures(display, result.features, text);
        }

        cv::Mat thresholdBgr;
        cv::cvtColor(result.binary, thresholdBgr, cv::COLOR_GRAY2BGR);
        std::map<std::string, std::vector<uchar>> encoded;
        cv::imencode(".jpg", display, encoded["annotated"], jpegParams);
        cv::imencode(".jpg", thresholdBgr, encoded["threshold"], jpegParams);
        cv::imencode(".jpg", regionsVis, encoded["regions"], jpegParams);

        {
            std::lock_guard lock(shared.m);
            shared.jpeg = std::move(encoded);
            shared.statusJson = statusJson();
            ++shared.frameId;
        }
        shared.frameReady.notify_all();
        runCommands();

        const auto now = std::chrono::steady_clock::now();
        const double dt = std::chrono::duration<double>(now - lastTick).count();
        lastTick = now;
        if (dt > 0) fps = fps == 0.0 ? 1.0 / dt : 0.9 * fps + 0.1 / dt;
        // Cameras pace themselves; stills would otherwise spin a core re-processing one image.
        if (isStill()) std::this_thread::sleep_for(100ms);
    }

    shared.running = false;
    mailbox.close();
    shared.frameReady.notify_all();
    http.stop();
    httpThread.join();
    return 0;
}
