// HTTP backend for the web GUI (D-13). Runs the same pipeline as live, but instead of OpenCV
// windows it serves MJPEG streams and a small JSON API on 127.0.0.1.
// Usage: server [--public] [--allow-origin host]... [--browser | camera-index | video-file |
//               stream-url | image-dir] [port]
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

#include "app/capture.h"
#include "app/embedding.h"
#include "app/frame_mailbox.h"
#include "app/frame_source.h"
#include "app/session.h"

namespace fs = std::filesystem;
using namespace std::chrono_literals;

namespace {

const std::string kModelPath = "data/models/resnet18-v2-7.onnx";
const std::vector<std::string> kViews = {"annotated", "threshold", "regions"};
// Morphology n and the min-area filter are in pixels, tuned on 640x480 captures (D-07, D-21),
// so larger browser frames are scaled down to this width before the pipeline sees them.
constexpr int kPipelineWidth = 640;
constexpr int kMaxFrameSide = 4096;

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
// --allow-origin adds a GUI hosted elsewhere (the Netlify frontend, D-26), over https only,
// including its "<branch>--" deploy subdomains.
bool isAllowedOrigin(const httplib::Request& req, const std::vector<std::string>& extraHosts) {
    if (!req.has_header("Origin")) return true;
    const std::string origin = req.get_header_value("Origin");
    static const std::regex local(R"(^http://(localhost|127\.0\.0\.1)(:[0-9]{1,5})?$)");
    if (std::regex_match(origin, local)) return true;
    static const std::regex scheme(R"(^https?://)");
    if (req.has_header("Host") &&
        std::regex_replace(origin, scheme, "") == req.get_header_value("Host"))
        return true;
    const std::string prefix = "https://";
    if (origin.rfind(prefix, 0) != 0) return false;
    const std::string originHost = origin.substr(prefix.size());
    static const std::regex branchLabel(R"(^[a-z0-9-]+--$)");
    for (const auto& h : extraHosts) {
        if (originHost == h) return true;
        if (originHost.size() > h.size() &&
            originHost.compare(originHost.size() - h.size(), h.size(), h) == 0 &&
            std::regex_match(originHost.substr(0, originHost.size() - h.size()), branchLabel))
            return true;
    }
    return false;
}

}  // namespace

int main(int argc, char** argv) {
    cv::utils::logging::setLogLevel(cv::utils::logging::LOG_LEVEL_WARNING);

    // --public (D-26): hosted demo. Listens on every interface, keeps visitors' DB changes in
    // memory only (the committed DB is never modified), disables Capture (it writes images),
    // and serves the static GUI from web/out so page, API and WebSocket share one origin.
    bool isPublic = false;
    std::vector<std::string> positional, allowedHosts;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--public")
            isPublic = true;
        else if (arg == "--allow-origin" && i + 1 < argc)
            allowedHosts.push_back(argv[++i]);
        else
            positional.push_back(arg);
    }
    const std::string spec = positional.size() > 0 ? positional[0] : "--browser";
    const int port = positional.size() > 1 ? std::stoi(positional[1]) : 8080;
    const std::string host = isPublic ? "0.0.0.0" : "127.0.0.1";
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
    http.set_pre_routing_handler([&](const httplib::Request& req, httplib::Response& res) {
        if (isAllowedOrigin(req, allowedHosts)) return httplib::Server::HandlerResponse::Unhandled;
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
    std::unique_ptr<Embedder> embedder;
    if (fs::exists(kModelPath)) {
        try {
            embedder = std::make_unique<Embedder>(kModelPath);
        } catch (const cv::Exception& e) {
            std::cerr << "ResNet18 unavailable: " << e.what() << "\n";
        }
    }
    SessionConfig config;
    config.persist = !isPublic;
    config.embeddingAvailable = embedder != nullptr;
    // The public demo opens on the method that recognises the samples (47/47 vs 26/47 on the
    // T7 split); visitors can still switch to hand features to compare.
    if (isPublic) {
        config.classifier = "resnet18";
        config.metric = "cosine";
    }
    Session session(config);
    bool advance = true;
    double fps = 0.0;

    // train adds to both DBs, so it needs this frame's embedding even in features mode.
    const auto embedCurrent = [&](std::vector<double>& out) {
        cv::Mat roi;
        if (!embedder || !session.currentRoi(roi)) return false;
        embedder->embed(roi, out);
        return true;
    };
    const auto onPipeline = [&](httplib::Response& res, std::function<Reply()> fn) {
        sendJson(res, runOnPipeline(shared, std::move(fn)));
    };

    http.Get("/api/db", [&](const httplib::Request&, httplib::Response& res) {
        onPipeline(res, [&] { return session.db(); });
    });

    http.Post("/api/db/reset", [&](const httplib::Request&, httplib::Response& res) {
        onPipeline(res, [&] { return session.resetDb(); });
    });

    http.Delete(R"(/api/db/([A-Za-z0-9_-]+))",
                [&](const httplib::Request& req, httplib::Response& res) {
                    const std::string target = req.matches[1];
                    onPipeline(res, [&, target] { return session.deleteLabel(target); });
                });

    http.Post("/api/label", [&](const httplib::Request& req, httplib::Response& res) {
        std::string next;
        extractString(req.body, "label", next);  // missing -> "" -> rejected by the session
        onPipeline(res, [&, next] { return session.setLabel(next); });
    });

    http.Post("/api/train", [&](const httplib::Request&, httplib::Response& res) {
        onPipeline(res, [&] {
            std::vector<double> embedding;
            const bool embedded = embedCurrent(embedding);
            return session.train(embedded ? &embedding : nullptr);
        });
    });

    // Capture stays here, not in Session: it writes image files, which only the local server
    // may do (D-26), and the browser build has nowhere to put them.
    http.Post("/api/capture", [&](const httplib::Request&, httplib::Response& res) {
        onPipeline(res, [&] {
            const std::string& label = session.label();
            if (label.empty()) return Reply{409, jsonError("set a label first")};
            if (isPublic) return Reply{403, jsonError("capture is disabled on the public demo")};
            if (session.frame().empty()) return Reply{409, jsonError("no camera frame yet")};
            const std::string file = timestamp() + ".png";
            saveImage(fs::path("data/objects") / label, file, session.frame());
            saveImage(fs::path("out/stages/threshold") / label, file, session.result().binary);
            saveImage(fs::path("out/stages/regions") / label, file, session.regionsView());
            saveImage(fs::path("out/stages/features") / label, file, session.annotated());
            return Reply{200, R"({"saved":"data/objects/)" + label + "/" + file + "\"}"};
        });
    });

    http.Post("/api/next", [&](const httplib::Request&, httplib::Response& res) {
        onPipeline(res, [&] {
            advance = true;
            return Reply{200, "{}"};
        });
    });

    http.Put("/api/settings", [&](const httplib::Request& req, httplib::Response& res) {
        double value = 0;
        extractNumber(req.body, "unknownThreshold", value);
        onPipeline(res, [&, value] { return session.setCutoff(value); });
    });

    http.Put("/api/classifier", [&](const httplib::Request& req, httplib::Response& res) {
        std::string classifier, metric;
        extractString(req.body, "classifier", classifier);
        extractString(req.body, "metric", metric);  // optional: "" = the classifier's default
        onPipeline(res, [&, classifier, metric] { return session.setClassifier(classifier, metric); });
    });

    http.Put("/api/mode", [&](const httplib::Request& req, httplib::Response& res) {
        std::string next;
        extractString(req.body, "mode", next);
        onPipeline(res, [&, next] { return session.setMode(next); });
    });

    http.Post("/api/autolearn/label", [&](const httplib::Request& req, httplib::Response& res) {
        std::string name;
        extractString(req.body, "label", name);
        onPipeline(res, [&, name] { return session.autoLearnLabel(name); });
    });

    http.Post("/api/autolearn/dismiss", [&](const httplib::Request&, httplib::Response& res) {
        onPipeline(res, [&] { return session.autoLearnDismiss(); });
    });

    http.Get("/api/eval", [&](const httplib::Request&, httplib::Response& res) {
        onPipeline(res, [&] { return session.evalMatrix(); });
    });

    http.Post("/api/eval/record", [&](const httplib::Request&, httplib::Response& res) {
        onPipeline(res, [&] { return session.evalRecord(); });
    });

    http.Delete("/api/eval", [&](const httplib::Request&, httplib::Response& res) {
        onPipeline(res, [&] { return session.evalReset(); });
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
              << "  (source: " << spec << ", ResNet18: " << (embedder ? "on" : "off") << ")"
              << std::endl;
    if (browser)
        std::cout << "waiting for the GUI camera on ws://127.0.0.1:" << port << "/ws/frames"
                  << std::endl;

    const std::vector<int> jpegParams = {cv::IMWRITE_JPEG_QUALITY, 80};
    auto lastTick = std::chrono::steady_clock::now();

    const auto runCommands = [&] {
        std::deque<std::pair<std::function<Reply()>, std::promise<Reply>>> pending;
        {
            std::lock_guard lock(shared.m);
            pending.swap(shared.commands);
        }
        for (auto& [fn, promise] : pending) promise.set_value(fn());
    };
    const auto statusJson = [&] {
        return session.statusJson({sourceName(), isStill(), browser, isPublic, fps});
    };

    cv::Mat frame;
    std::vector<double> embedding;
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

        session.process(frame, Session::Clock::now());
        const bool embedded = session.needsEmbedding() && embedder;
        if (embedded) embedder->embed(session.roi(), embedding);
        session.finish(embedded ? &embedding : nullptr);

        cv::Mat thresholdBgr;
        cv::cvtColor(session.result().binary, thresholdBgr, cv::COLOR_GRAY2BGR);
        std::map<std::string, std::vector<uchar>> encoded;
        cv::imencode(".jpg", session.annotated(), encoded["annotated"], jpegParams);
        cv::imencode(".jpg", thresholdBgr, encoded["threshold"], jpegParams);
        cv::imencode(".jpg", session.regionsView(), encoded["regions"], jpegParams);

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
