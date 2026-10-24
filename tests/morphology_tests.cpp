// Headless tests for the from-scratch morphology stage (D-05).
// Expected grids are hand-computed by you; a case whose `expected` is {} is reported as TODO.
// Also dumps out/tests/*.png so the 4- vs 8-connected shapes can be inspected by eye.
// Border policy under test: pixels outside the image count as background (D-08).

#include <filesystem>
#include <iostream>
#include <opencv2/imgcodecs.hpp>
#include <string>
#include <vector>

#include "core/morphology.h"

namespace fs = std::filesystem;
namespace orec = objectrecognition;
using orec::Connectivity;
using Grid = std::vector<std::vector<int>>;

namespace {

int failures = 0;
int todos = 0;

// Grids are written as 0/1 for readability; images follow the 0/255 convention (H-35).
cv::Mat toBinary(const Grid& g) {
    cv::Mat m(static_cast<int>(g.size()), static_cast<int>(g[0].size()), CV_8UC1);
    for (int r = 0; r < m.rows; ++r)
        for (int c = 0; c < m.cols; ++c) m.at<uchar>(r, c) = g[r][c] ? 255 : 0;
    return m;
}

// Binary outputs are compared as 0/1, distance outputs as raw ints.
int cellValue(const cv::Mat& m, int r, int c) {
    if (m.type() == CV_8UC1) return m.at<uchar>(r, c) ? 1 : 0;
    return m.at<int>(r, c);
}

void printSideBySide(const cv::Mat& actual, const Grid& expected) {
    std::cout << "    actual" << std::string(actual.cols * 3 - 3, ' ') << "  | expected\n";
    for (int r = 0; r < actual.rows; ++r) {
        std::cout << "    ";
        for (int c = 0; c < actual.cols; ++c) {
            const int a = cellValue(actual, r, c);
            const bool bad = r < static_cast<int>(expected.size()) && a != expected[r][c];
            std::cout << (bad ? '*' : ' ') << (a < 10 ? " " : "") << a;
        }
        std::cout << "  |";
        for (int v : expected[r]) std::cout << ' ' << (v < 10 ? " " : "") << v;
        std::cout << "\n";
    }
}

void check(const std::string& name, const cv::Mat& actual, const Grid& expected) {
    if (expected.empty()) {
        ++todos;
        std::cout << "[TODO] " << name << ": fill in the expected grid\n";
        return;
    }
    if (actual.empty()) {
        ++failures;
        std::cout << "[FAIL] " << name << ": output is empty (not implemented yet?)\n";
        return;
    }
    const int rows = static_cast<int>(expected.size());
    const int cols = static_cast<int>(expected[0].size());
    if (actual.rows != rows || actual.cols != cols) {
        ++failures;
        std::cout << "[FAIL] " << name << ": size " << actual.cols << "x" << actual.rows
                  << ", expected " << cols << "x" << rows << "\n";
        return;
    }
    if (actual.type() != CV_8UC1 && actual.type() != CV_32SC1) {
        ++failures;
        std::cout << "[FAIL] " << name << ": output type must be CV_8UC1 or CV_32SC1 (D-08)\n";
        return;
    }
    for (int r = 0; r < rows; ++r)
        for (int c = 0; c < cols; ++c)
            if (cellValue(actual, r, c) != expected[r][c]) {
                ++failures;
                std::cout << "[FAIL] " << name << " (* = mismatch)\n";
                printSideBySide(actual, expected);
                return;
            }
    std::cout << "[ OK ] " << name << "\n";
}

// ---- Inputs (0 = background, 1 = foreground) ----

const Grid kBlock3In5 = {
    {0, 0, 0, 0, 0}, {0, 1, 1, 1, 0}, {0, 1, 1, 1, 0}, {0, 1, 1, 1, 0}, {0, 0, 0, 0, 0},
};

const Grid kFull5 = {
    {1, 1, 1, 1, 1}, {1, 1, 1, 1, 1}, {1, 1, 1, 1, 1}, {1, 1, 1, 1, 1}, {1, 1, 1, 1, 1},
};

const Grid kHole7 = {
    // one background pixel in the centre of a 7x7 foreground
    {1, 1, 1, 1, 1, 1, 1}, {1, 1, 1, 1, 1, 1, 1}, {1, 1, 1, 1, 1, 1, 1}, {1, 1, 1, 0, 1, 1, 1},
    {1, 1, 1, 1, 1, 1, 1}, {1, 1, 1, 1, 1, 1, 1}, {1, 1, 1, 1, 1, 1, 1},
};

const Grid kDot5 = {
    {0, 0, 0, 0, 0}, {0, 0, 0, 0, 0}, {0, 0, 1, 0, 0}, {0, 0, 0, 0, 0}, {0, 0, 0, 0, 0},
};

void gridTests() {
    cv::Mat out;

    // Your hand-computed grid from the session.
    orec::grassfire(toBinary(kBlock3In5), out, Connectivity::Four);
    check("grassfire 4-conn: 3x3 block in 5x5", out,
          {
              {0, 0, 0, 0, 0},
              {0, 1, 1, 1, 0},
              {0, 1, 2, 1, 0},
              {0, 1, 1, 1, 0},
              {0, 0, 0, 0, 0},
          });

    out.release();
    orec::grassfire(toBinary(kBlock3In5), out, Connectivity::Eight);
    check("grassfire 8-conn: 3x3 block in 5x5", out, {});  // TODO: same as 4-conn? why?

    out.release();
    orec::grassfire(toBinary(kFull5), out, Connectivity::Four);
    check("grassfire 4-conn: full 5x5, border = bg", out, {});  // TODO

    out.release();
    orec::grassfire(toBinary(kHole7), out, Connectivity::Four);
    check("grassfire 4-conn: 7x7, bg at centre + border", out, {});  // TODO

    out.release();
    orec::grassfire(toBinary(kHole7), out, Connectivity::Eight);
    check("grassfire 8-conn: 7x7, bg at centre + border", out, {});  // TODO

    // Worked example: everything within distance 1 of background is removed.
    out.release();
    orec::shrink(toBinary(kBlock3In5), out, Connectivity::Four, 1);
    check("shrink 1, 4-conn: 3x3 block -> centre pixel", out,
          {
              {0, 0, 0, 0, 0},
              {0, 0, 0, 0, 0},
              {0, 0, 1, 0, 0},
              {0, 0, 0, 0, 0},
              {0, 0, 0, 0, 0},
          });

    out.release();
    orec::grow(toBinary(kDot5), out, Connectivity::Four, 1);
    check("grow 1, 4-conn: single pixel", out, {});  // TODO

    out.release();
    orec::grow(toBinary(kDot5), out, Connectivity::Eight, 1);
    check("grow 1, 8-conn: single pixel", out, {});  // TODO
}

// Large synthetic shapes where the diamond/square difference is obvious by eye.
void visualDump() {
    const fs::path dir = "out/tests";
    fs::create_directories(dir);

    cv::Mat dot = cv::Mat::zeros(121, 121, CV_8UC1);
    dot.at<uchar>(60, 60) = 255;
    cv::Mat square = cv::Mat::zeros(121, 121, CV_8UC1);
    square(cv::Rect(40, 40, 41, 41)).setTo(255);

    struct Case {
        std::string file;
        const cv::Mat* src;
        bool isGrow;
        Connectivity conn;
        int n;
    };
    const std::vector<Case> cases = {
        {"grow30_dot_four.png", &dot, true, Connectivity::Four, 30},
        {"grow30_dot_eight.png", &dot, true, Connectivity::Eight, 30},
        {"grow20_square_four.png", &square, true, Connectivity::Four, 20},
        {"grow20_square_eight.png", &square, true, Connectivity::Eight, 20},
        {"shrink15_square_four.png", &square, false, Connectivity::Four, 15},
        {"shrink15_square_eight.png", &square, false, Connectivity::Eight, 15},
    };

    for (const auto& tc : cases) {
        cv::Mat out;
        if (tc.isGrow)
            orec::grow(*tc.src, out, tc.conn, tc.n);
        else
            orec::shrink(*tc.src, out, tc.conn, tc.n);
        if (out.empty()) {
            std::cout << "[SKIP] " << (dir / tc.file).string() << ": output empty\n";
            continue;
        }
        cv::imwrite((dir / tc.file).string(), out);
        std::cout << "[DUMP] " << (dir / tc.file).string() << "\n";
    }
}

}  // namespace

int main() {
    gridTests();
    visualDump();
    std::cout << "\n" << failures << " failed, " << todos << " TODO\n";
    return failures == 0 ? 0 : 1;
}
