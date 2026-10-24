# Decision log

Every architectural / ownership decision: the options, the trade-offs you stated, the pick, and who builds it.
Owner: **HAND** = you write it, Claude reviews. **SNIPPET** = you type it from syntax hints, you choose the options. **AGENT** = Claude writes it to your spec.

Status: `open` → `decided` → (`superseded by D-xx`).

| ID | Date | Decision | Owner | Status |
| --- | --- | --- | --- | --- |
| D-00 | 2026-10-23 | Hand-code vs agent split | mixed | superseded by D-11 |
| D-01 | 2026-10-23 | Scope vs 2026-10-27 deadline | — | decided |
| D-02 | 2026-10-23 | Embedding model (native C++) | SNIPPET | decided (pending confirm) |
| D-03 | 2026-10-23 | Browser camera → inference → overlay | AGENT (you architect) | deferred (gated by D-01) |
| D-04 | 2026-10-23 | Repo / CMake layout | AGENT (you architect) | decided |
| D-06 | 2026-10-23 | Capture storage + labelling | AGENT (you architect) | decided |
| D-07 | 2026-10-23 | Camera source | — | proposed (test Sat AM) |
| D-08 | 2026-10-23 | Grassfire border policy + distance type | HAND | decided |
| D-09 | 2026-10-24 | T3 via OpenCV; time reinvested in extensions | SNIPPET | decided |
| D-10 | 2026-10-24 | Extension set | mixed | decided (metric open) |
| D-11 | 2026-10-24 | AI-first build: you hand-code T1 (done) + T2; agent builds the rest; you pass a review per part | mixed | decided |
| D-12 | 2026-10-24 | Feature DB format | AGENT | decided |
| D-13 | 2026-10-24 | Web GUI architecture (localhost) | AGENT (you architect) | **proposed: review before merge** |
| D-14 | 2026-10-24 | T7 evaluation protocol | AGENT | **proposed** |
| D-05 | 2026-10-23 | Headless tests + 4/8-conn visual dump for from-scratch stages | AGENT harness, HAND expected values | decided |

---

## D-00 — Hand-code vs agent split

**Status:** decided

| Area | Owner | Why |
| --- | --- | --- |
| T2 morphology via grassfire transform (from scratch) | HAND | Core CV concept; counts toward the from-scratch requirement |
| T3 connected components | SNIPPET (OpenCV, D-09) | Superseded: was region growing from scratch |
| T1 threshold (ISODATA + OpenCV calls) | SNIPPET | Learn which threshold option fits which situation |
| T4 moments, primary axis, oriented bbox | HAND (math), SNIPPET (drawing) | The axis math is what the post-grading Q&A will probe |
| T6 scaled Euclidean + per-feature stdev, unknown-object threshold | HAND | Feature scaling is a core ML/CV idea |
| T7 confusion-matrix tally | HAND | Small; evaluation reasoning |
| T9 rotate/crop/resize + ONNX inference | SNIPPET (instructor code, with attribution; you must be able to explain each line) | Learn the `cv::dnn` API without reinventing it |
| CMake, repo cleanup, CSV I/O, main loop, input sources, key handling, windows, CLI | AGENT | Mundane |
| Glue topics flagged to teach | Teach-in | Stable region colours across frames (centroid matching), `cv::Mat` ownership and copy cost |

## D-01 — Scope vs deadline

**Status:** decided. **Due 2026-10-28 02:59 Boston (EDT) = Tue 2026-10-27 23:59 Seattle (PDT).** Tuesday is a full working day.
**Decision:** graded deliverables first (T1–T9 + report + video). Extensions planned: 2 from-scratch stages + unknown-object detection. The web app is a stretch goal.
**Gate:** web work starts only after T1–T9, the report draft, and the video are done.
**Your trade-off:** the web app is "cherry on top" portfolio value; it's agent-heavy, so you expect to multitask.
**Mentor caveat:** architecting and reviewing agent work is not free. Budget ≥ 3h of your own time for it.
**Time available:** Fri 10/23 17:18–22:00 (−1h break, ≈3.7h) + 01:00–04:00 (3h); Sat 10/24 full day; Sun 10/25 off (RL quiz); Mon 10/26 = ?
**Still unanswered:** Monday availability; fallback if the web build fails.

## D-02 — Embedding model (native C++, Task 9)

**Status:** decided, pending your confirmation.
**Pick:** ResNet18 via `cv::dnn` + ONNX (512-d, global-average-pool output).
**Corrected reasoning:** instructor code already targets ResNet18, so the C++ path is proven, which saves hours. Its higher param count is a *cost*, not a benefit. Embedding length is irrelevant at this DB size (H-08).
**Later (web):** MobileNetV3-Small is the likely browser model (~10 MB, 0.06 GFLOPs). It gets a separate per-model DB (H-01).

## D-03 — Live browser pipeline

**Status:** deferred until the D-01 gate.
**Leaning:** hybrid. Classical pipeline in WASM, CNN in ONNX Runtime Web (webgpu → wasm fallback), static hosting on Netlify. "Add new object" dropped.
**Still to settle:** the real argument for hybrid (backends, not download size — H-02), the 1,000-user failure mode, the per-model DB schema, and COOP/COEP headers (H-09).

## D-04 — Repo / CMake layout

**Status:** decided. **Option B:** `objrec_core` (pure stages) + `objrec_app` (glue) + one lean executable per job (`live` now; `train`, `eval`, `tests` as they land).
**Your reasoning:** independence, lean executables. *Corrected (H-11):* load cost isn't the win; small focused mains are.
**Constraint (H-10):** `objrec_core` never links highgui/videoio.
**Build:** `cmake --preset msvc` then `cmake --build --preset debug`. Executables go to `out/`. MSVC is forced because the OpenCV prebuilt is MSVC-only.

## D-05 — Tests for from-scratch code

**Status:** decided. A `tests` executable with plain asserts (no framework). The agent writes the harness and a PNG dump comparing 4-conn and 8-conn grow/shrink. You hand-compute the expected grassfire values for at least one small grid. That's the learning part.
**Built:** `tests/morphology_tests.cpp` → `./out/tests.exe`. Prints OK/FAIL/TODO with side-by-side grids, and dumps grow/shrink PNGs to `out/tests/`. A case with `expected = {}` is a TODO for you to fill in by hand.

## D-06 — Capture storage + labelling

**Status:** decided.
- **Raw (source of truth, committed):** `data/objects/<label>/<timestamp>.png`. Folder = label, filename = ms timestamp, so captures are appended, never overwritten. This is the ImageNet-style convention, so batch training and Task 7 eval get the true label for free.
- **Derived (regenerable, ignored):** `out/stages/<stage>/<label>/<timestamp>.png`. Stage-first, so it's easy to pick report images per task.
- **Label entry:** option (b): press `n` and type in the terminal (task-suggested). **Fallback:** if the frozen video while typing is unusable, write the root cause here and switch to (a) `--label` CLI arg.
- **Object count:** 10. Base 5 + the "≥5 more objects" extension; it also shows where the features stop discriminating. Cost: a 10×10 confusion matrix to explain.
- **Your trade-off:** raw is committed because it can't be recaptured and the report needs it (~25 MB est.). Derived goes in out/ because it's cheap to regenerate.
- `data/captures/` (soft-toy test shots) deleted 2026-10-23.

## D-07 — Camera source

**Status:** proposed; test Saturday morning.
**Facts:** Android phone, same Wi-Fi as the laptop. Bluetooth ruled out (bandwidth; phones don't expose a BT camera).
**Proposed:** A, the IP Webcam app → `./out/live.exe http://<phone-ip>:8080/video`. No code change, and it's real-time, which gives a better demo video.
**Fallbacks:** B, Windows Phone Link connected camera (shows up as camera index 1/2). C, still photos into `data/objects/<label>/` (report must then say "image-directory system").
**Physical:** phone pointing straight down over white paper, diffuse light.

## D-08 — Grassfire border policy + distance type

**Status:** decided.
- **Border = background** (your call). Consequence: shrinking eats objects inward from the image edge. OpenCV's `erode` does the opposite by default (border = +∞). Task 3 discards border-touching regions anyway, so this mainly affects clutter like the cardboard.
- **Distance image = `CV_32S`**. Worst case is 240 at 640×480 but 360 at 1280×720, which overflows uchar. int maths is simplest, and the INF sentinel + 1 can't wrap (H-34).
- **Init:** bg = 0, fg = INF. Out-of-image neighbours read as 0, which implements the border policy (H-33).

## D-09 — T3 via OpenCV; reinvest time in extensions

**Status:** decided (initial pass).
**Decision:** T3 uses `cv::connectedComponentsWithStats`. Region growing from scratch only if time is left at the end.
**Your reasoning:** the base tasks cap at 26/30; extensions are worth the last 4, so hours go where the points are.
**From-scratch requirement still met:** T1 (ISODATA + applyThreshold, no `cv::threshold`) and T2 (grassfire) are both from scratch. ≥1 is required for a solo student, so 2 already qualifies for the "more than required from scratch" extension.
**Extension shortlist:** see the D-10 question in the session.

## D-10 — Extension set

**Status:** decided; the distance-metric item is open.
1. ✅ >1 stage from scratch: T1 + T2 (D-09). No extra work.
2. Unknown-object detection: a distance cutoff in T6 (HAND, ~0.5h).
3. +5 objects (10 total) (~1.5h).
4. Better GUI: an OpenCV panel in `live` with sliders + DB keys (AGENT to your spec, ~0.5h of yours). Chosen over the web app for the deadline.
5. **Open:** "embeddings with scaled Euclidean". Mentor objection: per-dim stdev over ~10 one-shot samples is noisy and blows up near-zero-variance dims. Alternatives: a metric comparison (scaled-Euclid vs cosine vs SSD), or a 2-D PCA plot.
- **Web app:** after the deadline (D-01 gate, D-03 design).
**Budget:** ~26h available (Sat ~10, Mon 15:00–24:00 ≈ 7, Tue ≈ 9 + 3h report buffer) vs ~25–27h needed. Zero slack, so the extensions are ordered by cost.

## D-11 — AI-first build (supersedes D-00)

**Status:** decided.
**Context:** you want to maximise the grade. Extensions = the last 4 of 30 points. task.pdf is silent on AI.
- **HAND:** T1 (done: ISODATA + applyThreshold) and T2 grassfire. This covers the from-scratch requirement and the ">1 from scratch" extension.
- **AGENT:** T3–T7, T9, the GUI, and the extensions. You specify, then review, then pass a 2-minute explain-it-back check per part (Q&A = 2 pts).
- **Attribution:** cite AI use in report §5 (Acknowledgements); the session transcript gets committed to the repo.
- **Extensions (revises D-10):**
  1. Auto-learning unknown objects (includes unknown detection).
  2. Measured invariance: synthetic rotation/scale sweeps with a plot of each feature's drift.
  3. Better GUI. You want it web-based → see D-03 for the options. The OpenCV panel stays as the guaranteed fallback.
  4. >1 stage from scratch (T1 + T2).
  5. +5 objects (from D-10). *Not re-listed in round 2: confirm.*

## D-12 — Feature DB format

**Status:** decided. Option (a): one headerless CSV, `label,f1,...,fk`, one row per sample.
**Your reasoning:** nobody opens it by hand; if the features change, regenerate the DB (YAGNI).
**Consequence:** the feature order is a code contract. If the features change, the DB must be retrained (stale rows → wrong distances). Embeddings get a separate file per model (H-01).

### D-11 build log

- **2026-10-24 ~01:00 (agent): T3–T6 built.**
  - `core/regions` (OpenCV CC, min-area filter, largest-N, renumbering, central-and-off-border primary).
  - `core/features` (3-pass moments → θ, oriented bbox from axis extents, 5 features: percentFilled, aspectRatio, hu1–hu3).
  - `core/classifier` (per-feature stdev, scaled Euclidean, nearest neighbour + unknown cutoff).
  - `app/feature_db` (D-12 CSV), `app/pipeline` (T1→T4, with a T2 slot), `app/overlay`.
  - `live` keys: `t` = train; `inspect` = headless report images + `out/stages/features.csv`.
  - Verified on the dev set: OBB and axis align on img4P3; cardboard is excluded as a border region; img1p3 (yellow) finds no object, as predicted from grayscale ≈ 227.
  - **Pending your review checks.**
- **2026-10-24 overnight (agent), branch `stack/objrec`**, oldest → newest. Review one commit at a time:
  1. `9b6c2b4` core: regions / features / classifier (T3–T6)
  2. `0ccdd07` app: live + inspect wiring
  3. `f1bfbad` docs: D-09..D-12
  4. `dd8af2f` refactor: shared capture helpers + label validation
  5. `7204d1c` server.exe: MJPEG + JSON API (D-13, proposed)
  6. `5ea4c25` web scaffold (generated; skim only)
  7. `fb36a7d` web dashboard (D-13, proposed)
  8. `421e915` train + eval, leave-one-out (D-14, proposed)
  9. `013af69` invariance extension
  - Not merged to main. Your WIP `src/core/morphology.cpp` is untouched and uncommitted; an older copy sits in `stash@{0}`.

## D-13 — Web GUI architecture (localhost)

**Status:** PROPOSED. Built overnight on `stack/objrec` while you slept; nothing is merged. Reject or amend at review.
**Your constraints:** latest stable Next.js + TypeScript + Tailwind + shadcn/ui; long class lists wrapped in custom class names; counts as the "better GUI" extension. Public web/Netlify/WASM stays post-deadline (D-03).

**Shape:** `server.exe` (C++, the same runPipeline as live) ⇄ HTTP ⇄ Next.js app in `web/`.

| Choice | Picked | Alternatives and why not |
| --- | --- | --- |
| Video transport | **MJPEG** (`multipart/x-mixed-replace`) | WebSocket JPEG frames: needs JS decode→canvas, more code, no gain on loopback. WebRTC: lowest latency but signalling + codecs; overkill locally. Polling snapshots: simplest, but per-frame request overhead + latency. |
| C++ HTTP lib | **cpp-httplib v0.60.1** (header-only, MIT, vendored) | Crow/Drogon: frameworks with build deps. Boost.Beast: Boost on Windows = hours of setup. |
| Status updates | **Poll `/api/status` every 250 ms** | SSE/WebSocket push: fresher, but another streaming endpoint for a few JSON fields. |
| CORS | **Next rewrites** `/api/*` → `127.0.0.1:8080`; `<img>` loads the stream directly from :8080 | CORS headers in C++: fine too, but rewrites keep the browser same-origin; rewrite proxies may buffer multipart, so the stream bypasses it. |
| Concurrency | **Command queue → pipeline thread** (handlers wait on a promise) | Mutex around the DB/label: every handler then races the frame loop. |

**Security:** binds 127.0.0.1 only (the API writes files and deletes DB rows). Labels are validated as `[A-Za-z0-9_-]{1,64}` (blocks `../` traversal). httplib is pinned past every published GHSA (latest fix: 0.51.0).
**Verified:** curl smoke test on the dev set: status, label validation, next, settings, db, a 2 s stream (~224 KB), bad view → 400.
**Questions for you:** (1) MJPEG vs WebSocket: agree? (2) Is 250 ms polling acceptable? (3) Should the GUI also expose auto-learn (extension 1) once it's built?

## D-14 — T7 evaluation protocol

**Status:** PROPOSED (built overnight).
**Default:** leave-one-out over `data/objects/<label>/`. Each image is classified against all the *other* images.
**Why:** evaluating against a DB that contains the test image scores 100% by self-match (distance 0). Shown on the dev set: fixed DB 6/6 vs LOO 2/6.
**Alternatives:** (b) `--db` = the interactively trained DB vs separately captured test images. Honest only if the test images were never trained on. (c) a fixed train/test split per label: needs ≥ 2× the captures.
**Your call:** which protocol goes in the report? LOO needs ≥ 2 images per label (≥ 3 recommended, per Task 7).
