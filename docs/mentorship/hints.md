# Hints given

Each hint gets an ID. If the same hint is needed again → −1 on the scoreboard, and its `Repeats` count goes up.

| ID | Date | Topic | Hint (short) | Repeats |
| --- | --- | --- | --- | --- |
| H-01 | 2026-10-23 | Embeddings | Embeddings from different models live in different spaces. They can't be compared even at the same length, so key the DB by model. | 1 |
| H-02 | 2026-10-23 | Web arch | Hybrid = WASM *plus* an ONNX model, so its download is never smaller than pure WASM. Argue from backends (WebGPU/SIMD), not from size. | 0 |
| H-03 | 2026-10-23 | Browser perf | A tab "hangs" because inference runs on the main thread. Move it to a Web Worker: lower fps, but no freeze. | 0 |
| H-04 | 2026-10-23 | Async | Results that are still in flight when the model switches are stale. Tag every request with a model/generation id and drop results that don't match. | 0 |
| H-05 | 2026-10-23 | Morphology | Opening = shrink→grow (removes specks and thin spurs). Closing = grow→shrink (fills holes and gaps). Glare holes inside the object → closing. | 0 |
| H-06 | 2026-10-23 | Grassfire | Backward pass: d = min(d, min(down, right) + 1). `self` gets no +1. 4-conn → city-block → diamond; 8-conn → chessboard → square. | 1 |
| H-07 | 2026-10-23 | Feature scaling | Range (max−min) is set by 2 extreme points, so one outlier squashes that feature. Stdev uses every point, so it's more robust. | 0 |
| H-08 | 2026-10-23 | Embeddings | With N≈10 DB entries, vector length (512 vs 1280) costs nothing. It only matters at large N (memory, ANN indexes). | 0 |
| H-09 | 2026-10-23 | Web | ORT Web backends: webgpu → wasm fallback. Multi-threaded WASM needs COOP/COEP headers (cross-origin isolation). | 0 |
| H-10 | 2026-10-23 | Architecture | Stage functions are pure cv::Mat→cv::Mat with no imshow/VideoCapture. Reasons: (1) they can compile to WASM, where there are no windows or cameras (the browser supplies frames); (2) they can be unit-tested headless. | 0 |
| H-11 | 2026-10-23 | Build | Separate executables don't save load cost; they all link the same core and share OpenCV DLLs. The real benefit is small mains and a clear purpose for each one. | 0 |
| H-12 | 2026-10-23 | C++ / cv::Mat | cv::Mat = small header + reference-counted pixel buffer (like shared_ptr). Returning one copies only the header, and memory is freed when the last ref drops: no disk, no leak. Out-param wins because `dst.create()` is a no-op when size/type already match, so there's zero allocation per frame. | 1 |
| H-13 | 2026-10-23 | Thresholding | ISODATA recomputes the threshold every frame, so it tracks *global* lighting changes. It's still one threshold per image, so a *local* shadow can still break it. | 0 |
| H-14 | 2026-10-23 | C++ headers | .h = declarations (what callers see), .cpp = definitions. `#pragma once` stops double inclusion. Wrap in a namespace. TS has no equivalent: `import` does both jobs. | 1 |
| H-15 | 2026-10-23 | C++ includes | `#include "core/threshold.h"` (quotes, path relative to include/) for your own headers. `<...>` is for system/library headers like `<opencv2/core.hpp>`. | 0 |
| H-16 | 2026-10-23 | C++ includes | Include what you use, where you use it: the header mentions cv::Mat in its signature, so the header itself must include `<opencv2/core.hpp>`. | 1 |
| H-17 | 2026-10-23 | Design | YAGNI: "swappable method" in C++ doesn't need DI machinery (std::function / interfaces). Separate *compute t* from *apply t*, and swapping methods = calling a different function for t. | 1 |
| H-18 | 2026-10-23 | OpenCV | OpenCV stores colour as **BGR**, not RGB. The call is `cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY)` (needs `<opencv2/imgproc.hpp>`). | 1 |
| H-19 | 2026-10-23 | Thresholding | Finding the valley in a raw histogram is fragile: noise creates many local minima. ISODATA = k-means with K=2 on intensities: assign each sample to the nearer mean, recompute the means, repeat. t = midpoint. | 0 |
| H-20 | 2026-10-23 | Binary images | Convention: foreground = 255, background = 0 (a value of 1 is invisible in imshow). The object is dark, so pixel < t → 255. | 1 |
| H-21 | 2026-10-23 | Perf | k-means on scalars doesn't need cluster *sets*. Keep a running sum + count per cluster, so there's no allocation per iteration. | 0 |
| H-22 | 2026-10-23 | Numerics | Never stop a loop on `double == double`. Use \|Δ\| < ε (0.5 is enough here, since t is an int). | 1 |
| H-23 | 2026-10-23 | OpenCV pixels | `const uchar* row = gray.ptr<uchar>(r); row[c]` beats `gray.at<uchar>(r,c)`: one address computation per row instead of per pixel, and no debug-build bounds checks. | 0 |
| H-24 | 2026-10-23 | Sampling | Spatial stride s in both row and col samples 1/s² of the pixels: s=4 → 1/16. Histogram bins (256/16) are an unrelated concept. | 0 |
| H-25 | 2026-10-23 | Thresholding | Sampling is only for *estimating* t. applyThreshold must write every pixel. `create()` doesn't zero memory, so skipped pixels hold garbage. | 0 |
| H-26 | 2026-10-23 | C++ style | Implementation constants go in the .cpp as `constexpr int kSampleStep = 4;` inside an anonymous namespace, not in the public header. The header is the contract, not the internals. | 1 |
| H-27 | 2026-10-23 | C++ numerics | Always write `std::abs` with `<cmath>`. Unqualified `abs` can resolve to the C `int abs(int)`, which silently truncates doubles on some compilers. | 0 |
| H-28 | 2026-10-23 | Loop logic | continue = (still moving) **&&** (budget left). Moving = ΔD > ε **\|\|** ΔL > ε. De Morgan: stop when converged \|\| out of budget ⇔ continue when !converged && budget. | 0 |
| H-29 | 2026-10-23 | Shell | In zsh/bash, `\` is an escape character (`out\live.exe` → `outlive.exe`). Use forward slashes: `./out/live.exe data/examples`. | 0 |
| H-30 | 2026-10-23 | Evaluation | Confusion matrix = K×K table of *counts*: row = true label, column = predicted label. The diagonal is correct; off-diagonal cells show which classes get mistaken for which. It's not the data matrix. | 0 |
| H-31 | 2026-10-23 | PCA preview | Data matrix: one image vector per column (m×N). When N ≪ m (e.g. 50 images of 4096 px), eigen-decompose the small N×N matrix AᵀA instead of the huge m×m AAᵀ (the "eigenfaces trick"). | 0 |
| H-32 | 2026-10-23 | Grassfire | Raster order explains the passes: forward (top-left → bottom-right) may only read neighbours *already visited*: up, left (8-conn adds up-left, up-right). Backward (bottom-right → top-left) reads down, right (8-conn adds down-right, down-left). d = min(d, min(neighbours) + 1). | 0 |
| H-33 | 2026-10-23 | Grassfire | The distance image is not the binary image. Init: bg → 0, fg → INF, so min() can only lower it. The value used for out-of-image neighbours *is* the border policy: 0 = border counts as bg, INF = it doesn't. | 0 |
| H-34 | 2026-10-23 | Types | Worst case with border = bg: min(W,H)/2 (240 at 640×480; 360 at 1280×720 → overflows uchar). Without border = bg, a corner bg pixel gives up to W+H. Use CV_32S: plain int maths, and INF + 1 can't wrap. | 0 |
| H-35 | 2026-10-23 | OpenCV | There's no "binary" type in OpenCV: a binary image is a `CV_8UC1` cv::Mat holding only 0/255 by convention. Enforce it with the parameter name + `CV_Assert(type == CV_8UC1)` + a doc comment. | 0 |
| H-36 | 2026-10-23 | C++ syntax | Parameters are `Type name` (TS is `name: Type`). There are no `frame[][]` array params for images: pass `const cv::Mat&`. | 1 |
| H-37 | 2026-10-23 | C++ syntax | `enum class Connectivity { Four, Eight };` (note the `;`). Scoped (`Connectivity::Four`) and no implicit int conversion, unlike a plain `enum`, which leaks names and converts silently. Types are PascalCase. | 0 |
| H-38 | 2026-10-23 | Project style | One project namespace, `objectrecognition`, for every core file. Callers write `orec::grassfire`, not a different namespace per file. | 1 |
| H-39 | 2026-10-23 | C++ syntax | Every C++ function needs an explicit return type (`void` if it returns nothing). TS infers one; C++ errors with C4430 "missing type specifier". | 0 |
| H-40 | 2026-10-23 | C++ const | Inputs get `const cv::Mat&`, outputs get plain `cv::Mat&`; a const output can't be `create()`d or written. Top-level `const` on by-value params (`const Connectivity conn`) means nothing in a declaration. | 0 |
| H-41 | 2026-10-24 | OpenCV 5 | 4.x → 5.0 moved `getRotationMatrix2D`, `boundingRect` (and other 2-D geometry) from imgproc to `<opencv2/geometry.hpp>`. Expect the same when porting the instructor's T9 code. | 0 |
