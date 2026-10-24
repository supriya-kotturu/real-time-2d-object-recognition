// HTTP backend for the web GUI (D-13). Runs the same pipeline as live, but instead of OpenCV
// windows it serves MJPEG streams and a small JSON API on 127.0.0.1.
// Usage: server [camera-index | video-file | stream-url | image-dir] [port]   (default: 0 8080)
//
//   GET    /stream?view=annotated|threshold|regions   multipart MJPEG
//   GET    /api/status                                 latest frame's threshold, features, match
//   GET    /api/db                                     sample count per label
//   DELETE /api/db/<label>                             drop every sample with that label
//   POST   /api/label     {"label": "scissors"}
//   POST   /api/train                                  append current features under the label
//   POST   /api/capture                                save raw + stage images (D-06)
//   POST   /api/next                                   advance to the next still image
//   PUT    /api/settings  {"unknownThreshold": 3.0}
//
// Threading: the pipeline, the DB, and the label are owned by the main thread. HTTP handlers
// never touch them; they queue a command and wait for its reply, so no locks around the DB.

#include <httplib.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <functional>
#include <future>
#include <iostream>
#include <map>
#include <mutex>
#include <opencv2/core/utils/logger.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <regex>
#include <sstream>
#include <thread>

#include "app/capture.h"
#include "app/feature_db.h"
#include "app/frame_source.h"
#include "app/overlay.h"
#include "app/pipeline.h"
#include "core/classifier.h"

namespace fs = std::filesystem;
namespace orec = objectrecognition;
using namespace std::chrono_literals;

namespace {

const std::string kDbPath = "data/db/features.csv";
const std::vector<std::string> kViews = {"annotated", "threshold", "regions"};

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
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
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
    if (!std::regex_search(body, m,
                           std::regex("\"" + key + "\"\\s*:\\s*(-?[0-9]+(?:\\.[0-9]+)?)")))
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

}  // namespace

int main(int argc, char** argv) {
    cv::utils::logging::setLogLevel(cv::utils::logging::LOG_LEVEL_WARNING);

    const std::string spec = argc > 1 ? argv[1] : "0";
    const int port = argc > 2 ? std::stoi(argv[2]) : 8080;
    FrameSource source(spec);
    if (!source.isOpen()) {
        std::cerr << "cannot open input: " << spec << "\n";
        return 1;
    }

    Shared shared;
    httplib::Server http;

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
                    shared.frameReady.wait_for(lock, 1s, [&] {
                        return shared.frameId != last || !shared.running;
                    });
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
    double unknownThreshold = 3.0;
    std::vector<orec::Sample> db = loadFeatureDb(kDbPath);
    std::vector<double> stdevs = orec::featureStdevs(db);
    FrameResult result;
    cv::Mat frame, display, regionsVis;
    bool advance = true;

    const auto dbJson = [&] {
        std::map<std::string, int> counts;
        for (const auto& s : db) ++counts[s.label];
        std::ostringstream out;
        out << R"({"total":)" << db.size() << R"(,"labels":[)";
        bool first = true;
        for (const auto& [name, count] : counts) {
            out << (first ? "" : ",") << R"({"label":")" << jsonEscape(name)
                << R"(","count":)" << count << "}";
            first = false;
        }
        out << "]}";
        return out.str();
    };

    http.Get("/api/db", [&](const httplib::Request&, httplib::Response& res) {
        sendJson(res, runOnPipeline(shared, [&] { return Reply{200, dbJson()}; }));
    });

    http.Delete(R"(/api/db/([A-Za-z0-9_-]+))", [&](const httplib::Request& req,
                                                  httplib::Response& res) {
        const std::string target = req.matches[1];
        sendJson(res, runOnPipeline(shared, [&, target] {
            const auto before = db.size();
            db.erase(std::remove_if(db.begin(), db.end(),
                                    [&](const orec::Sample& s) { return s.label == target; }),
                     db.end());
            if (db.size() == before) return Reply{404, jsonError("no such label")};
            saveFeatureDb(kDbPath, db);
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
            const orec::Sample sample{label, result.features.values};
            appendSample(kDbPath, sample);
            db.push_back(sample);
            stdevs = orec::featureStdevs(db);
            return Reply{200, dbJson()};
        }));
    });

    http.Post("/api/capture", [&](const httplib::Request&, httplib::Response& res) {
        sendJson(res, runOnPipeline(shared, [&] {
            if (label.empty()) return Reply{409, jsonError("set a label first")};
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
            unknownThreshold = value;
            return Reply{200, cv::format(R"({"unknownThreshold":%.3f})", value)};
        }));
    });

    // Loopback only: the API can write files and delete DB rows, so it must not face the LAN.
    std::thread httpThread([&] {
        if (!http.listen("127.0.0.1", port)) {
            std::cerr << "cannot listen on 127.0.0.1:" << port << "\n";
            shared.running = false;
        }
    });
    std::cout << "serving on http://127.0.0.1:" << port << "  (source: " << spec << ", db: "
              << db.size() << " samples)" << std::endl;

    const std::vector<int> jpegParams = {cv::IMWRITE_JPEG_QUALITY, 80};
    double fps = 0.0;
    auto lastTick = std::chrono::steady_clock::now();

    while (shared.running) {
        // Stills are re-processed each tick so settings changes show up without a new frame.
        if (!source.isStill() || advance) {
            cv::Mat next;
            if (!source.next(next)) {
                if (!source.isStill()) break;  // video ended / camera lost
            } else {
                frame = next;
            }
            advance = false;
        }
        if (frame.empty()) break;

        runPipeline(frame, result);
        frame.copyTo(display);
        colorizeRegions(result.regionMap, regionsVis);

        std::ostringstream status;
        status << R"({"threshold":)" << result.threshold << R"(,"label":")" << jsonEscape(label)
               << R"(","source":")" << jsonEscape(source.currentName()) << R"(","still":)"
               << (source.isStill() ? "true" : "false") << R"(,"dbSize":)" << db.size()
               << R"(,"unknownThreshold":)" << unknownThreshold << R"(,"fps":)"
               << cv::format("%.1f", fps) << R"(,"object":)";
        if (result.hasFeatures) {
            std::string text = "object";
            status << R"({"features":{)";
            for (size_t i = 0; i < result.features.values.size(); ++i)
                status << (i ? "," : "") << '"' << orec::kFeatureNames[i]
                       << "\":" << result.features.values[i];
            status << R"(},"prediction":)";
            if (db.size() >= 2) {
                const auto match = orec::classifyNearest(db, stdevs, result.features.values,
                                                         unknownThreshold);
                text = match.known ? match.label : "unknown";
                status << R"({"label":")" << jsonEscape(match.label) << R"(","distance":)"
                       << match.distance << R"(,"known":)" << (match.known ? "true" : "false")
                       << "}";
            } else {
                status << "null";
            }
            status << "}";
            drawFeatures(display, result.features, text);
        } else {
            status << "null";
        }
        status << "}";

        cv::Mat thresholdBgr;
        cv::cvtColor(result.binary, thresholdBgr, cv::COLOR_GRAY2BGR);
        std::map<std::string, std::vector<uchar>> encoded;
        cv::imencode(".jpg", display, encoded["annotated"], jpegParams);
        cv::imencode(".jpg", thresholdBgr, encoded["threshold"], jpegParams);
        cv::imencode(".jpg", regionsVis, encoded["regions"], jpegParams);

        std::deque<std::pair<std::function<Reply()>, std::promise<Reply>>> pending;
        {
            std::lock_guard lock(shared.m);
            shared.jpeg = std::move(encoded);
            shared.statusJson = status.str();
            ++shared.frameId;
            pending.swap(shared.commands);
        }
        shared.frameReady.notify_all();
        for (auto& [fn, promise] : pending) promise.set_value(fn());

        const auto now = std::chrono::steady_clock::now();
        const double dt = std::chrono::duration<double>(now - lastTick).count();
        lastTick = now;
        if (dt > 0) fps = fps == 0.0 ? 1.0 / dt : 0.9 * fps + 0.1 / dt;
        // Cameras pace themselves; stills would otherwise spin a core re-processing one image.
        if (source.isStill()) std::this_thread::sleep_for(100ms);
    }

    shared.running = false;
    shared.frameReady.notify_all();
    http.stop();
    httpThread.join();
    return 0;
}
