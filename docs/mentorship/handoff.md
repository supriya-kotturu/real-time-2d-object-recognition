# Session handoff (2026-10-24 ~18:00, end of session 1)

Read this first, then `decisions.md` (D-00..D-25), `hints.md` (H-01..H-49), `scoreboard.md`.

## Deadline and time

- **Due Tue 2026-10-27 23:59 Seattle time** (2026-10-28 02:59 Boston). Sun 10/25 is off (RL quiz). Mon 10/26 15:00–24:00 is available; Tue is a full day, with the **last 3 h reserved for report + submission**.
- Grade: base 26 + extensions 4 (30). Q&A afterwards = 2 pts: the user must explain everything.

## The mentorship protocol (from the user, still in force)

- Staff C++/CV mentor. The user is a C++ beginner with a TypeScript background; syntax hints are welcome.
- **+1** for visible improvement, **−1** when a hint already in `hints.md` has to be given again. Log every point in `scoreboard.md` (ledger + total) *in the same turn*. One was missed this session and fixed late.
- **Progress update every 6 h** of work: next due ~2026-10-25 00:00 of *active* time (i.e. during Monday's session).
- Non-C++ work (web, deploy) is agent work, **but the user architects it**: present options and trade-offs, and let them choose before building. Log decisions in `decisions.md` with their stated trade-off.
- D-11/D-20: the agent now builds most code. The user hand-wrote T1 (ISODATA + applyThreshold). T2 grassfire was written by the agent from the user's pseudocode (D-20), so **explain-it-back checks are the condition** for claiming it.
- The user's global CLAUDE.md: terse, Q1/Q2 tags for questions, "Unresolved questions" after plans, sparring/skeptic on architecture.

## Repo state

- Branch **`stack/objrec`**, 32+ commits ahead of `main`, **nothing merged** (the user reviews stacked commits first; never merge without approval). `main` = the user's own commits up to `e3e0185`.
- `git stash@{0}`: an old copy of morphology.cpp WIP; obsolete, the user may `git stash drop`.
- Commits are **GPG-signed**; when the gpg-agent cache expires, `git commit` blocks on a passphrase dialog. Tell the user, and never bypass signing.

## Architecture (D-04)

`objrec_core` (pure cv::Mat stages, no highgui/videoio) → `objrec_app` (glue) → one exe per job in `out/`:

| exe | purpose |
| --- | --- |
| `live <src>` | OpenCV windows; keys: `n` type label **in the window** + Enter, `s` save raw+stages, `t` train, `q` quit |
| `server [--browser\|<src>] [port]` | backend for the web GUI. Default `--browser`: frames arrive from the GUI camera on WS `/ws/frames` (D-24); `<src>` = IP Webcam URL / index / file / dir. MJPEG `/stream?view=`, JSON `/api/*`. 127.0.0.1 only + Origin allowlist (localhost) |
| `inspect [dir]` | headless: out/stages/{threshold,cleaned,regions,features,roi}/ + features.csv (report images T1–T4, T9 A–C) |
| `train [objects] [db]` | rebuild data/db/features.csv from data/objects/<label>/ |
| `eval [test] [--train dir] [--one-shot] [--db f] [--unknown x]` | confusion matrices for hand features **and** ResNet18 embeddings on the same split → out/eval/confusion_<method>_<protocol>.{md,csv} |
| `invariance [dir]` | extension: rotation/scale drift per feature → out/invariance/ |
| `tests`, `autolearn_tests`, `embedding_tests` | test suites (all pass; 6 TODO grids intentionally skipped, D-05) |

Pipeline (`src/app/pipeline.cpp`): saturation darkening `V·(1−S)` (D-22) → ISODATA (user's T1) → closing n=2 then opening n=1, 8-conn (D-21, proposed values) → OpenCV connected components, central off-border region (T3, D-09) → moments/axis/OBB + 5 features `percentFilled, aspectRatio, hu1, hu2, hu3` (T4) → scaled-Euclidean NN with stdev floor (T6) / cosine NN on ResNet18 embeddings (T9, `app/embedding.cpp`, ROI aligned by +θ with third-moment 180° disambiguation).

Web GUI: `web/` = Next.js 16.4.3 + TS + Tailwind v4 + shadcn/ui (Base UI). Long class lists are named classes in `globals.css`; generated `components/ui/*` untouched. Modes Detect (auto-learn, D-15/D-17) / Collect / Evaluate. Run: `./out/server.exe` (browser camera) or `./out/server.exe "<src>"`, then `cd web && npm run dev` (binds 127.0.0.1) → http://localhost:3000 → Start camera. npm audit: 0 vulns.

## Data

- `data/objects/<label>/` 58 training images, 10 labels: ball, batteries, cloud, keychain, kitchen-towel, mouse, notebook, spects, sticky-notes, toy-car (640×480).
- `data/test/<label>/` 38 test images, 8 labels: **keychain and spects test captures still missing**.
- `data/rejected/` 7 with README. `data/models/resnet18-v2-7.onnx` gitignored (URL + SHA-256 in `data/models/README.md`).
- Camera (D-07): Android **IP Webcam** → `"http://USER:PASS@10.0.0.11:8080/video"` (http, not https; credentials in URL; 640×480 in the app).

## Results so far (D-23)

| Protocol | Hand features | ResNet18 embedding |
|---|---|---|
| split (DB = data/objects, test = data/test), T7 | 18/37 = 48.6% | 37/37 = 100% |
| one-shot split, T9 | 13/37 = 35.1% | 34/37 = 91.9% |
| leave-one-out on data/objects | 32/58 = 55.2% | 57/58 = 98.3% |

Blob-like objects (ball, batteries, cloud, towel) are confused; ball → sticky-notes (disc and square both lack a defined least-moment axis). Embeddings win via colour and texture.

## Open work, in priority order

1. **D-24 browser camera (user: "we need it now").** Ask the open questions in D-24 first (which camera: the laptop webcam faces the user; the phone browser needs HTTPS + non-loopback binding). Then build: getUserMedia → canvas JPEG → `POST /api/frame` → server `--browser` push source (latest-frame mailbox) → same pipeline.
2. **D-25 classifier toggle** (features | ResNet18) in the GUI: confirm the design with the user, then build (`train --embed`, per-model embedding DB, per-method cutoff).
3. User: capture 3 test poses each for **keychain** and **spects** (`test_keychain`, `test_spects` labels in live/Collect), move them to `data/test/<label>/`, re-run `train` + both `eval` splits.
4. User: **explain-it-back checks**: grassfire ×3 (why grow's outside = far; why bg init fits in the forward loop; why the 8-conn forward pass never reads DOWN_LEFT), T3 (central off-border vs largest), T4 (μ11; OBB from extents vs minAreaRect), T6 (stdev floor vs skipping). Also the closing-gap numbers question (3 px gap, n=2) → D-21 final values.
5. **T8 demo video** (live or GUI; classical + embedding). **Report PDF**: offer a skeleton pre-filled with the required images/tables from out/ (T1–T4 same 2–3 images; one labelled result per class; confusion matrices; T9 comparison; extensions: >1 stage from scratch, +5 objects with impact discussion, auto-learn, measured invariance, GUI, metric comparison). readme.txt: name, video link, time-travel days. Acknowledge AI use plus the instructor utilities (third_party/maxwell).
6. Merge `stack/objrec` → `main` **only after the user reviews**.
7. Post-deadline: public Netlify hybrid version (D-03, D-19: build-time model fetch + SHA-256 + immutable cache; Git LFS question open).

## Gotchas that cost time this session

- `LNK1168`: an exe is still running; quit it before building.
- Builds sometimes sat for minutes: most likely the GPG prompt or the VS Code CMake Tools lock; check `tasklist` before assuming a hang.
- **Never** `taskkill //IM node.exe`: it kills VS Code's processes. Kill by PID only.
- zsh: use `./out/x.exe`, forward slashes (backslash is an escape).
- Python heredoc edits: use raw strings `r'''…'''`, or `\n` in C++ string literals turns into real newlines.
- OpenCV 5: `getRotationMatrix2D`/`boundingRect` live in `<opencv2/geometry.hpp>` (H-41).
- Use the **Release** build for live use (`cmake --build --preset release`); Debug is several times slower.
- The user may be editing `src/core/*` concurrently; check `git status` / file changes before writing, and never stash their WIP again.
