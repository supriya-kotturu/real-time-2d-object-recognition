#include "app/capture.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <opencv2/imgcodecs.hpp>
#include <sstream>

std::string timestamp() {
    using namespace std::chrono;
    const auto now = system_clock::now();
    const std::time_t secs = system_clock::to_time_t(now);
    const auto ms = duration_cast<milliseconds>(now.time_since_epoch()).count() % 1000;
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &secs);
#else
    localtime_r(&secs, &local);
#endif
    std::ostringstream out;
    out << std::put_time(&local, "%Y%m%d-%H%M%S") << '-' << std::setw(3) << std::setfill('0')
        << ms;
    return out.str();
}

void saveImage(const std::filesystem::path& dir, const std::string& file, const cv::Mat& img) {
    std::filesystem::create_directories(dir);
    cv::imwrite((dir / file).string(), img);
}

bool isValidLabel(const std::string& label) {
    return !label.empty() && label.size() <= 64 &&
           std::all_of(label.begin(), label.end(), [](unsigned char c) {
               return std::isalnum(c) || c == '_' || c == '-';
           });
}
