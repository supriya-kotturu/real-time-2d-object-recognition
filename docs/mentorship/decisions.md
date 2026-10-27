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
| D-07 | 2026-10-23 | Camera source | — | decided: IP Webcam over Wi-Fi, working |
| D-08 | 2026-10-23 | Grassfire border policy + distance type | HAND | revised 2026-10-24 (shrink: border = far) |
| D-09 | 2026-10-24 | T3 via OpenCV; time reinvested in extensions | SNIPPET | decided |
| D-10 | 2026-10-24 | Extension set | mixed | decided (metric open) |
| D-11 | 2026-10-24 | AI-first build: you hand-code T1 (done) + T2; agent builds the rest; you pass a review per part | mixed | decided |
| D-12 | 2026-10-24 | Feature DB format | AGENT | decided |
| D-13 | 2026-10-24 | Web GUI architecture (localhost) | AGENT (you architect) | **proposed: review before merge** |
| D-14 | 2026-10-24 | T7 evaluation protocol | AGENT | decided: task protocol (b) |
| D-15 | 2026-10-24 | GUI modes + auto-learn | AGENT (you architect) | decided |
| D-16 | 2026-10-24 | YOLO vs ResNet18 for T9 | SNIPPET | decided: ResNet18 |
| D-17 | 2026-10-24 | Auto-learn: per-pose capture instead of a frame burst | AGENT (you architect) | decided: B motion-gated, built |
| D-18 | 2026-10-24 | T9 port of instructor utilities + model handling | AGENT | decided, built |
| D-19 | 2026-10-24 | Model delivery for the Netlify (browser) version | AGENT (you architect) | decided: build-time fetch + cache (LFS question open) |
| D-20 | 2026-10-24 | T2 grassfire body written by agent from your pseudocode | AGENT (your design) | decided; explain-it-back pending |
| D-21 | 2026-10-24 | T2 cleanup parameters | you | proposed: close 2 / open 1, 8-conn |
| D-22 | 2026-10-24 | Saturation pre-processing for coloured objects | AGENT | decided, built (option C) |
| D-23 | 2026-10-24 | First evaluation results (T7 + T9) | — | recorded |
| D-24 | 2026-10-24 | Browser camera (getUserMedia) for the local GUI now | AGENT (you architect) | decided: build now; design questions open |
| D-25 | 2026-10-24 | GUI classifier toggle: hand features vs ResNet18 embedding | AGENT | proposed; awaiting confirmation |
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

### D-14 update (2026-10-24)

**Decided: follow the task.** Train a DB, then evaluate on ≥ 3 *separate* images per object in new positions/orientations, using a true-label hook. → `eval data/test --db data/db/features.csv`; test captures go in `data/test/<label>/`. LOO stays as a supplementary number.

## D-13 update — your WebRTC proposal

You proposed: webcam → server over WebRTC media tracks; results back over an unreliable DataChannel (or drawn onto a return track).
**Mentor objection:** that's the right transport *for server-side processing*, but it contradicts D-03, where you chose hybrid/client-side compute so that no frames leave the browser. Under hybrid there's no server to stream to, so WebRTC isn't needed at all. Under server-side, Netlify can't host it (static + short functions, no long-lived media sessions); you'd need a media server (libwebrtc/GStreamer/aiortc), TURN for NAT traversal (paid bandwidth), and one server-side decode + pipeline per user.
**Open:** pick the deployment world first (client-side hybrid vs server-side processing); the transport follows from it. The localhost GUI stays MJPEG either way (loopback, single user).

## D-15 — GUI modes + auto-learn (proposed)

- **Detect:** live recognition. Auto-learn: an object that stays *unknown* (distance > cutoff in every frame) and *roughly stationary* (centroid drift small) for T seconds triggers a "name this object" prompt. On a label → auto-collect N samples over ~2 s into the DB ("collect statistics", per the extension text). "Not registered anywhere" = no DB entry within the cutoff for the whole window; a dismissed object isn't re-prompted until it leaves the frame.
- **Collect:** set label → train / capture (current controls).
- **Evaluate (suggested):** set the true label → "record" tallies a live confusion matrix (the task's "hook to tell the system if it's correct").
- **Open:** T = 10 s (yours) vs ~3 s; N samples; Evaluate as a third mode?

## D-16 — YOLO vs ResNet18 for T9 (open)

See session: YOLO is a detector over fixed (COCO) classes, not a one-shot embedding; Task 9 requires an embedding + one-shot NN + a comparison vs the hand-built features. Ultralytics YOLO is AGPL-3.0.

### Decisions 2026-10-24 (morning)

- **D-03/D-13:** the public (Netlify) version processes **in the browser** (hybrid, client compute). No WebRTC: frames never leave the browser. The localhost GUI keeps MJPEG.
- **D-15:** three modes: **Detect** (auto-learn after **3 s** unknown + stationary), **Collect**, **Evaluate** (live confusion matrix via a true-label hook).
- **D-16:** ResNet18 one-shot embedding for T9. YOLO rejected: per-class retraining cost, no one-shot, no Netlify compute budget, AGPL. Still open: instructor's ONNX (Canvas) vs a public ONNX-zoo ResNet18.

## D-17 — Auto-learn per-pose capture (open)

**Your insight:** 5 frames 0.4 s apart of a still object are near-identical: no pose diversity, ~0 stdev (the root cause behind the stdev-floor fix in `89fe735`).
**Options:** (A) a "Capture pose" button per pose. (B) motion-gated: after each sample, require a move ≥ ~10% of the frame or a rotation ≥ ~20°, then stillness, then auto-capture. Hands stay free for the demo.

## D-18 — T9 port + model handling (open)

- **Model:** `data/models/resnet18-v2-7.onnx` (ONNX zoo, 46.8 MB, sha256 `3b4d0480…824b`). Probe: OpenCV 5 loads it with both ENGINE_CLASSIC and ENGINE_NEW; `forward("onnx_node!resnetv22_flatten0_reshape0")` → 1×512 on both.
- **Instructor utilities.cpp issues:** (1) includes the prof's features.h/vision.h; (2) `M_PI` needs `_USE_MATH_DEFINES` on MSVC; (3) the debug `cv::rectangle` is drawn on the image *before* the ROI crop, so a 4 px gray frame leaks into every embedding; (4) the E2 "up" convention must be verified against our y-down extents; (5) `getRotationMatrix2D` → `<opencv2/geometry.hpp>` in OpenCV 5; (6) its comment says to embed the thresholded image, but Task 9 says the original.
- **Proposed:** port to `app/embedding.{h,cpp}` with attribution plus a rotation test; move the original unmodified to `third_party/maxwell/`; gitignore `*.onnx` with a README link + sha256.

### D-18 outcome (2026-10-24)

Decided: the original lives unmodified in `third_party/maxwell/` as reference; our own OpenCV 5 utils are in `app/embedding.{h,cpp}`; `*.onnx` is gitignored with URL + SHA-256 in `data/models/README.md`.
Built: `c90d385` (ROI + embedder + cosine; mutation-checked test), `58a570c` (eval `--one-shot` + features-vs-embedding comparison; inspect writes `out/stages/roi/`).

## D-19 — Model delivery for the browser version (open, post-deadline)

The model isn't in git, so Netlify's build must get it some other way. Options are in the session: build-time fetch + SHA-256 check; Git LFS; external CDN fetched at runtime; plus a smaller/quantised model for the browser.

### D-17 / D-19 decisions (2026-10-24)

- **D-17:** B, auto-capture on move. Built: a new pose needs ≥ 10% diagonal move or ≥ 20° rotation (undirected axis), then 0.6 s still; 5 poses; Stop = keep samples + no re-prompt until the object leaves.
- **D-19:** ResNet18 fetched at Netlify build time (URL + SHA-256 from `data/models/README.md`), served with a content-hashed filename and `Cache-Control: immutable` so returning users don't re-download. **Open:** Git LFS instead. You like its workflow fit; how Netlify builds treat LFS objects is unverified (check Netlify's docs), and builds pulling LFS objects use GitHub's LFS bandwidth quota.

## D-20 — T2 implementation handed to the agent (amends D-11)

**Status:** decided 2026-10-24, at your request ("fill it in based on what I commented").
**Trade-off stated:** the requirement is still met by T1 (ISODATA + applyThreshold, hand-written). The ">1 stage from scratch" extension and the Q&A now rest on you being able to explain grassfire, so the explain-it-back check is the condition.
**Implemented as you specified:** forward pass assigns `min(UP, LEFT [, UP_LEFT, UP_RIGHT]) + 1` with bg/fg init in the same loop; backward `min(curr, min(DOWN, RIGHT [, DOWN_LEFT, DOWN_RIGHT]) + 1)`; shrink removes `dist <= n`.
**Where it differs from your comments:** (1) grow runs grassfire on the *inverted* image and keeps `dist <= n` (your comment had `>= n` on the original); (2) grow treats outside-the-image as *far* (rows + cols), not as background. Otherwise every bg pixel within n of the image edge would turn fg, because in the inverted image the D-08 border policy means "the border is original foreground".
**Verified:** tests.exe 0 failed; out/tests shows the diamond (4-conn) vs square (8-conn).

### D-05 amendment (2026-10-24, 14:50)

Hand-computing the 6 TODO grids is skipped for time. The two hand-checked cases (3×3 grassfire, shrink 1) pass, and the 4- vs 8-conn behaviour is verified visually in out/tests. The TODO cases stay in tests.exe as reported TODOs.

## D-21 — T2 cleanup parameters (proposed)

Wired: closing (grow, shrink) then opening (shrink, grow), 8-conn, close n=2, open n=1.
Evidence: img4P3's blade glare gap (~6–8 px) narrows but doesn't seal at n=2 (it needs n ≥ ~4). Options: raise n (merges nearby parts, rounds corners), accept the gap, or decide on your own captures. Your report's T2 reasoning paragraph = this decision.

### D-07 outcome (2026-10-24 afternoon)

Working: IP Webcam (Android) → `./out/live.exe "http://USER:PASS@<ip>:8080/video"`. Pitfalls hit: `https` fails (the app serves http; its optional HTTPS uses a self-signed cert that FFmpeg rejects); auth enabled in the app → credentials go in the URL (URL-encode special characters). Report: real-time system (phone stream).

### D-06 fallback triggered (2026-10-24 afternoon)

**Root cause:** pressing `n` then typing in the terminal didn't work. OpenCV only sees keys while its window has focus, and the zsh terminal didn't hand `std::getline` input to the native .exe reliably.
**Fix (better than option a):** the label is typed into the video window itself: `n` → type → Enter (Esc cancels, Backspace edits). The video keeps running, and confirmations show on screen as well as in the terminal. `s` now also saves the cleaned stage.

### D-08 revisited (2026-10-24 afternoon)

**Evidence:** a mouse capture cut off by the frame still produced features. Cause: with border = bg, the closing's shrink(2) followed by the opening's grow(1) left every object 1 px off the image edge, so T3's border-touching filter never fired.
**Your decision:** in shrink, outside-the-image counts as **far** (the same as grow; matches `cv::erode`). Objects touching the edge stay touching. `grassfire()` itself still reports the border-as-bg distance.
**Guard:** regression test "shrink 1 keeps an edge-touching object touching the edge"; mutation-checked (fails with the old policy).
**Data:** you deleted the cut-off capture `mouse/20261024-153855-956.png`.

## D-22 — Saturation pre-processing (Task 1 suggestion)

**Context:** 2026-10-24 audit. Several objects are coloured, not dark (orange ball, pink towel/sticky notes, tan notebook, orange battery band), so plain grayscale only segmented their dark text or shadows. Replacement objects aren't available (shared studio).
**Decision:** option C: saturation darkening first (`out = V·(1 − S)`; paper and shadows have S≈0 and keep their brightness), then replace only what still fails.
**Before → after:** batteries, sticky notes and notebook text-only segmentations are gone; ball hu1 0.178 → 0.168 (a disc is 0.159, so now it's the whole ball, not ball + shadow).
**Rejected:** 5 mouse (1080p, too close, touch edge), notebook ×2 (opened / touches edge). See `data/rejected/README.md`.
**Known limitations (report material):** (1) squares/discs have no defined least-moment axis, so their OBB tilts arbitrarily and percentFilled varies (sticky notes 0.66); (2) articulated objects (keychain ring, spectacle legs) change silhouette per pose; (3) 7 of 10 objects are blob-like (hu1 ≈ 0.17–0.20), so expect confusion among them. That's the "impact of adding objects" discussion the extension asks for.

## D-23 — First evaluation results (2026-10-24 ~17:30)

Data: 58 training images (10 labels) in `data/objects/`; 37 usable test images (8 labels: keychain and spects test captures missing) in `data/test/`. Test outliers kept on purpose: curating a test set by hardness inflates accuracy.

| Protocol | Hand features (scaled Euclidean NN) | ResNet18 embedding (cosine NN) |
|---|---|---|
| split: DB = data/objects, test = data/test (T7) | 18/37 = 48.6% | 37/37 = 100% |
| split one-shot: 1 image/label as DB (T9) | 13/37 = 35.1% | 34/37 = 91.9% |
| leave-one-out over data/objects | 32/58 = 55.2% | 57/58 = 98.3% |

**Reading:** distinct silhouettes (toy-car, sticky-notes, notebook, mouse) are recognised; the blob-like objects (ball, batteries, cloud, towel) are not. Ball → sticky-notes 4/6: disc and square both lack a defined least-moment axis, so their OBB features coincide. The embedding wins because it sees colour and texture, which the 5 shape features ignore by design. Matrices: `out/eval/confusion_*_{split,split-one-shot,loo}.md`.

## D-24 — Browser camera for the local GUI (decided 2026-10-24 ~18:00: "we need it now")

**Constraint found:** `getUserMedia` only runs on secure contexts (`localhost` or HTTPS). Laptop webcam on `localhost:3000` works but faces the user, not down at the paper. Phone browser → `http://<laptop-ip>:3000` is blocked (not a secure context), and server.exe binds 127.0.0.1 only (D-13).
**Proposed design:** browser `getUserMedia` → `<video>` → canvas → JPEG → `POST /api/frame` (binary, ~10 fps, latest-frame-wins, drop stale) → server.exe "push" source (new `--browser` mode, a FrameSource alternative with a single-slot mailbox) → same pipeline → existing MJPEG/status. A camera picker (`enumerateDevices`) lets an external/USB webcam be chosen.
**Open questions for you (ask first in the next session):** (1) which camera: laptop webcam, a USB webcam, or the phone's browser (that needs HTTPS: a self-signed/mkcert cert plus binding server.exe beyond loopback, both of which weaken D-13's security)? (2) WebSocket vs POST per frame? (3) Keep IP Webcam as an alternative source?

## D-25 — Classifier toggle in the GUI (proposed)

Toggle features | embedding (ResNet18) in the GUI. Embedding DB per model (H-01): `data/db/embeddings_resnet18.csv` built by `train --embed`. Separate unknown cutoff per method (cosine 0–2 vs scaled Euclidean). Train/Collect/auto-learn append to both DBs. Compute embeddings only when needed (or every frame for instant switching, at an fps cost). Suits T8 ("classical, embedding, or both"). You asked about it; not confirmed yet.

### D-24 answers (session 2, 2026-10-24)

- **Camera:** the browser asks for camera permission (getUserMedia) and the user picks the device; not tied to one camera.
- **Transport:** WebSocket. (Correction: vendored cpp-httplib v0.60.1 has `Server::WebSocket`, so no extra library. Next rewrites don't proxy WS upgrades, so the client connects to ws://127.0.0.1:8080 directly, like the MJPEG stream.)
- **Sources:** browser camera becomes the default; IP Webcam moves to a separate branch "eventually".
- **Your question:** "since we're deploying on Netlify, where will the camera server be?" Open, see the session.

### D-24 decided (session 2, 2026-10-24)

- **World:** option A. Local now: browser camera → WebSocket `/ws/frames` → server.exe on 127.0.0.1 → same pipeline → MJPEG/status. Netlify (post-deadline, D-03) reuses the capture UI and swaps the sink for in-browser WASM. Rejected: B (WASM now: 1–2 days, puts T8/report at risk), C (hosted server.exe: frames leave the browser, costs money, public API needs auth).
- **Sources:** browser camera is the default (`server` with no args); IP Webcam/camera index/video/dir stay as options (`server <src>`). Your trade-off: "okay to give different options". Bookmark branch `archive/ipwebcam` (at `db84296`) is kept and never deleted.
- **Flow control (H-50):** one frame in flight. The server acks a frame only once the pipeline has consumed it; the browser sends the next frame on the ack. A single-slot mailbox (latest wins) covers several tabs.
- **Open:** hosting server.exe "for a limited time" (your follow-up); options in the session.

## D-25 decided (session 2, 2026-10-24)

- Embedding computed **only while ResNet18 is selected** (no fps cost in features mode).
- Train / Collect / auto-learn append to **both** DBs (`data/db/features.csv`, `data/db/embeddings_resnet18.csv`, keyed per model, H-01); `train --embed` rebuilds the embedding DB.
- **Metric dropdown:** Features → scaled Euclidean; ResNet18 → cosine (default) | SSD. One unknown cutoff per metric, defaults from eval's correct-match distances (one-shot split max: features 3.5 → 3.0 kept, cosine 0.36 → 0.35, SSD 522 → 500), sliders stay.
- Evidence (T7 split incl. keychain/spects, 47 tests): features 26/47, cosine 47/47, SSD 45/47.

## D-26 — Public deployment (decided 2026-10-24)

**Your call:** deploy before the deadline so the URL can go in the readme and you can talk about it; the course doesn't mention URLs; local + IP Webcam stay as the fallback.
**Shape (B):** Docker on Hugging Face Spaces (your account). OpenCV 5 built from source, server.exe serves the static Next export at `/` (one origin), model fetched at build with SHA-256 (D-19).
**Public mode (`--public`):** Capture (saving images) disabled; every other feature works for every visitor. DB writes are memory-only (shared until restart, "Reset DB" button), so the repo's DB is never modified. One camera connection at a time. A sample-images source so graders without your objects still see recognition work.
**Kill switch:** no container serving sample images by hour 3 → stop, fall back to video + readme.
**Mentor's objection (logged):** a URL isn't a submission requirement; graders without the objects see "unknown"; 4–6 h of risk against ~10 h of graded work.

### D-25 built + finding (2026-10-24 ~18:30)

Commits `5b7734d`..`c643d34`. ResNet18 costs ~40 ms/frame (features ~10 ms).
**Report finding (T9 discussion):** a uniform +10 grey-level brightness shift (Chrome's fake camera) flipped eval's hardest toy-car test image (cosine 0.26, the split's max correct distance) to keychain. The embedding is exposure-sensitive; shape features are thresholded and much less so. Real auto-exposure varies more, so expect the live accuracy to sit below the 47/47 split score.

## D-27 — Netlify without a server: pipeline in the browser (decided 2026-10-24 evening)

**Context:** HF Docker Spaces now need PRO (402 on create); free Docker hosts are 0.1 CPU or need a card.
**Your call:** "W1" — compile the C++ pipeline (OpenCV 5 core/imgproc/geometry + objrec_core + ROI) to WASM with Emscripten, run it plus onnxruntime-web (ResNet18) in a Web Worker, classify in TS; Netlify only. "Stick with it; willing to burn midnight oil."
**Trade-off stated by the mentor:** ~1 day with an unknown (OpenCV 5 WASM build on Windows), none of the 30 points depend on it; chosen over W2 (TS rewrite: two implementations that can disagree with the report) and paid/slow servers.
**Kept:** the same C++ code as the report numbers; the `FrameSink` seam (a `WasmSink` replaces `ServerSink`).
**Open:** the probing question (what must happen to a frame before ResNet18 can embed it?) is still unanswered.

### D-27 built (2026-10-24, night)

Commits `f645167` (Session extracted from server.exe), `ea6729d` (C++ → WebAssembly: OpenCV 5 core/imgproc/geometry via emsdk 3.1.74 in Docker), `11b7e02` (Web Worker: WASM pipeline + onnxruntime-web 1.30.0), Netlify config.
**Equivalence (the reason for W1 over W2):** same PNGs through server.exe and the WASM module give identical thresholds, features (relative diff 0), predictions and distances; the browser's ResNet18 embedding matches OpenCV dnn's (cosine distance 8e-8).
**Model:** fetched from the ONNX zoo's LFS host (github.com's redirect has no CORS header), SHA-256 checked, embedding declared as an extra output by a 46-byte protobuf splice (ORT only returns declared outputs). Not re-hosted.
**Speed in the browser:** hand features ~10 fps; ResNet18 ~3.6 fps (~270 ms/frame, single-threaded wasm; threads would need COOP/COEP, H-09).
**Deployed (draft):** https://6add760903fe7c39632e08fc--real-time-object-recognition.netlify.app, verified with headless Chrome (samples correct under ResNet18). Production URL waits for main (your review), via GitHub-connected builds.
**Superseded:** the HF Space plan (D-26 hosting; Docker Spaces need PRO). `server --public`, the Dockerfile and `--allow-origin` remain for self-hosting.

### Report finding: phone test (2026-10-24, night, deployed build)

A phone (not in the DB) was labelled "ball"/"cloud". Your diagnosis from the Threshold/Regions views: saturation darkening (D-22) makes the hand foreground; hand + phone form one region that touches the border, so T3 drops it (D-08); the camera module (a dark disc inside a lighter ring) becomes the primary region. Its ResNet18 distance (~0.35-0.375) straddles the cosine cutoff (0.35), so the label flickers between ball, cloud and unknown. NN is closed-set: it can only say a DB label or "unknown".
Fixes: demo protocol (object on paper, hands out of frame: the task's assumption); auto-learn the phone (extension demo); optionally a stricter cutoff (~0.30, costs false unknowns: one-shot correct matches reach 0.36).

## D-28 — GUI polish and browser storage (decided 2026-10-24, night)

- **Name:** Silhouette (your pick; mentor noted it undersells the ResNet18 half). Logo: a dark blob crossed by its primary axis.
- **Theme:** dark by default, **Lab bench**: near-black ink, lime accent (= OBB box), blue (= primary axis). The video overlay uses the same colours with a dark halo so it reads on white paper (your contrast question).
- **Storage:** samples a visitor trains survive a refresh: IndexedDB (the browser's built-in database), on their device only. Each sample = label + 5 features + 512-d embedding + a thumbnail of the **crop it learned from** (the ROI, region of interest). **Cap 50** (one auto-learn run = 5); a gallery lists them with delete-one / delete-label / clear-all; Train refuses when full.

### Real-device test (2026-10-25 ~01:30, your phone, deployed draft)

Phone layout fine; back camera; trained "phone" with 3 samples in the browser; left the site and came back: samples retained (IndexedDB); recognised the phone on a pale mint, patterned bedsheet, i.e. off the white-paper assumption, with ISODATA adapting to the background. Report material (extensions: GUI + browser deployment; T9 one-shot on a new object). Caveat to check: a sample whose crop is only the camera ring teaches the wrong shape; the gallery thumbnail shows it.

## D-29 — Smooth live view + ResNet18 on the GPU (decided 2026-10-25)

**Problem (yours, on the phone):** the live feed looks laggy. Cause: the picture shown *was* the processed frame, so it refreshed at recognition speed (~3.6 fps with ResNet18 on desktop, slower on phones).
**Chosen: A + B.** (A) show the camera directly and draw only the box/axis/label on top from the latest result: smooth video, overlay trails a moving object briefly. (B) run ResNet18 on WebGPU with a WebAssembly fallback. Rejected: embedding every Nth frame (stale labels; auto-learn could store stale embeddings).

**D-29 on your phone (2026-10-25):** classifier card reports "Running on your GPU via WebGPU" on the deployed draft.

### Report finding: auto-learn rarely fires on look-alikes (2026-10-27, demo recordings)

New objects whose silhouette resembles a trained one (round tin vs batteries, snack bag vs notebook at distance 0.301 / cutoff 0.350) flicker between a known label and unknown. Cause: one "known" frame resets the 3 s hold ([auto_learn.cpp](../../src/app/auto_learn.cpp) `update`), so a borderline object never stays unknown for 3 s straight. Workaround used in the demo: cutoff lowered to 0.200 (costs false unknowns on trained objects under lighting changes).
**Further improvement (yours, not built):** decide on the fraction of frames in the hold window that read unknown (e.g. ≥ 80%) instead of requiring every frame; one noisy frame then no longer erases the hold. Related: hysteresis (separate enter/leave cutoffs) or smoothing the distance over frames.
