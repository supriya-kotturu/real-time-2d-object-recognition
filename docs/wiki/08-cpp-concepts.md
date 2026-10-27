# 8. C++ concepts used in the code

The hints you were given most often, as a quick reference (IDs from
[hints.md](../mentorship/hints.md)).

## cv::Mat is a shared handle (H-12)

A `cv::Mat` is a small header plus a **reference-counted** pixel buffer, like a `shared_ptr`.
`cv::Mat b = a;` copies the header, not the pixels: both point at the same buffer. Use
`a.copyTo(b)` or `a.clone()` for a real copy. Memory is freed when the last handle goes away.

**Out-parameters:** stage functions take the output as `cv::Mat& dst` and call `dst.create(...)`.
`create` does nothing when the size and type already match, so a buffer reused frame after frame
is allocated once, not 30 times a second.

## const references for inputs (H-36, H-40)

Inputs are `const cv::Mat&`: no copy, and the function promises not to modify them. Outputs are
plain `cv::Mat&` (a const output couldn't be created or written).

## Headers, declarations, namespaces (H-14, H-15, H-16, H-38)

- `.h` declares what callers can use; `.cpp` defines it. `#pragma once` prevents double inclusion.
- `#include "core/threshold.h"` for your own headers; `<opencv2/core.hpp>` for libraries.
- A header includes what *its own declarations* use (cv::Mat in a signature → include core.hpp).
- One project namespace, `objectrecognition` (aliased `orec`).

## Types and numbers (H-22, H-27, H-34, H-42)

- Never compare doubles with `==`; use |a − b| < ε.
- `std::abs` from `<cmath>`; unqualified `abs` may pick the int version and truncate.
- Signed overflow (`INT_MAX + 1`) is undefined behaviour; pick an INF with room (rows + cols).
- The distance image is `CV_32S`: a binary image is `CV_8UC1` with values 0/255 by convention.

## Other

- `enum class Connectivity { Four, Eight };` is scoped and doesn't convert to int silently.
- Implementation constants go in the `.cpp` as `constexpr` in an anonymous namespace, not in the
  header.
- Row pointers (`gray.ptr<uchar>(r)`) are faster than `at<uchar>(r, c)` per pixel.

## Check yourself

<details><summary>What goes wrong with `cv::Mat display = frame;` followed by drawing on display?</summary>

Both share the same pixels, so the drawing also lands in `frame`, e.g. in the image you save as
training data. Use `frame.copyTo(display)`.
</details>

<details><summary>Why an out-parameter instead of returning a cv::Mat?</summary>

Returning is cheap (only the header is copied), but a new buffer is allocated each call. With an
out-parameter reused across frames, `create()` keeps the existing buffer: zero allocations per
frame.
</details>

<details><summary>Why is INF = INT_MAX a bug in grassfire?</summary>

The passes compute min(neighbours) + 1; INT_MAX + 1 overflows (undefined behaviour, in practice
wraps to a huge negative number), and min() then picks it as the "distance".
</details>
