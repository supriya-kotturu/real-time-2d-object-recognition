# Scoreboard

Rules:
- **+1** — a visible improvement (cleaner reasoning, a bug caught on your own, a better trade-off call, idiomatic C++ where before it wasn't).
- From D-11 on, agent-built parts earn/lose points in review: each part gets an explain-it-back check.
- **−1** — a hint already given (listed in `hints.md`) has to be given again.
- Progress update every 6h of active work. Measured from the `Last update` timestamp below; checked at the start of every session.

**Total: +13**
**Last update:** 2026-10-24 ~18:00 (checkpoint 2). Next one due after 6 more hours of work.

## Ledger

| Date | Δ | Reason | Ref |
| --- | --- | --- | --- |
| 2026-10-23 | 0 | Baseline | — |
| 2026-10-23 | +1 | A6: asked for verification of an AI-assistant claim (MobileNet vs ResNet18) instead of accepting it | D-02 |
| 2026-10-23 | −1 | H-01 repeated: the DB sketch had no model key (Comparable interface ≠ DB schema) | H-01 |
| 2026-10-23 | +1 | Re-scoped realistically: graded deliverables first, web app as a stretch goal | D-01 |
| 2026-10-23 | −1 | H-14 repeated: header had no declaration/namespace; the .cpp had no definition | H-14 |
| 2026-10-23 | +1 | Fixed declaration/definition split, namespace, and quoted include for own header | H-14, H-15 |
| 2026-10-23 | −1 | H-16 repeated: header uses cv::Mat without including <opencv2/core.hpp>, so the build failed (C2653) | H-16 |
| 2026-10-23 | +1 | Self-contained header; objrec_core compiles clean (only expected unused-param warnings on stubs) | H-16 |
| 2026-10-23 | −1 | H-17 repeated: pseudocode put the apply-loop (step 3) inside isodataThreshold right after splitting compute/apply | H-17 |
| 2026-10-23 | +1 | Pseudocode is now real ISODATA/k-means (assign → update → repeat, midpoint), with two stop conditions incl. an iteration cap | H-19 |
| 2026-10-23 | −1 | H-18 repeated: comment still says RGB / colorCvt | H-18 |
| 2026-10-23 | −1 | H-20 repeated: applyThreshold polarity inverted (p > t → 255 makes the paper the foreground) | H-20 |
| 2026-10-23 | +1 | applyThreshold fixed (every pixel, correct polarity); running sum/count instead of cluster sets; STEP=4 | H-20, H-21, H-24, H-25 |
| 2026-10-23 | −1 | H-26 repeated: STEP + new THRESHOLD constant still in the public header | H-26 |
| 2026-10-23 | +1 | Bug #2 found + explained: do-while condition = "still need to continue", flipped to continue while means move | — |
| 2026-10-23 | +1 | Bug #5: guarded empty clusters by keeping the previous mean (no NaN) | — |
| 2026-10-23 | +1 | Bug #4: returns the midpoint; std::abs applied | H-27 |
| 2026-10-23 | −1 | H-22 repeated: new early-break uses `prevD == D && prevL == L` (exact double equality) | H-22 |
| 2026-10-23 | +1 | D-06: chose the task-suggested option with an explicit fallback + "record the root cause" exit criterion | D-06 |
| 2026-10-23 | +1 | Bug #6 fixed: loop continues while moving && under the iteration cap | H-28 |
| 2026-10-23 | +1 | Explained why `display = frame` aliases the buffer (overlay would land in the saved frame) vs copyTo deep copy | H-12 |
| 2026-10-23 | +1 | Hand-computed 4-conn grassfire on a 5×5 grid correctly | D-05 |
| 2026-10-23 | −1 | H-06 repeated: passes swapped (forward = down/left, backward = up/right/self + 1) | H-06 |
| 2026-10-23 | +1 | Applied the explanation: marked d ≤ 2 → quarter-diamond (4-conn) vs 3×3 square (8-conn) | H-32 |
| 2026-10-23 | +1 | Reasoned the worst-case distance geometrically (deepest point is the centre, bounded by the shorter side) | D-08 |
| 2026-10-23 | −1 | H-12 repeated: grassfire declared as return-by-value in the per-frame path (out-param was chosen in Task 1) | H-12 |
| 2026-10-23 | +1 | morphology.h: correct `enum class` with `;`, out-params adopted, binary-convention comment | H-35, H-37 |
| 2026-10-23 | −1 | H-36 repeated: inputs passed as non-const `cv::Mat&` | H-36 |
| 2026-10-23 | −1 | Namespace still `morphology` after being asked to keep `objectrecognition` (logged as H-38) | H-38 |
| 2026-10-23 | +1 | morphology.h compiles: void return types, const inputs, project namespace, OpenCV-style param order | H-36, H-38, H-39 |
| 2026-10-24 | +1 | Scoped by grading value: swapped from-scratch T3 for OpenCV to free time for extensions (base caps at 26/30) | D-09 |
| 2026-10-24 | +1 | DB format: rejected the header row via YAGNI on your own ("no one opens it; change the code if needed") | H-17 |
| 2026-10-24 | +1 | Designed GUI modes (detect w/ auto-learn prompt vs collect) that map directly onto the task's evaluation hook + auto-learn extension | D-15 |
| 2026-10-24 | +1 | Rejected YOLO on concrete costs (per-class retraining, no server compute on Netlify) instead of hype | D-16 |
| 2026-10-24 | +1 | Spotted that a burst of near-identical frames gives no pose diversity (also the root cause of the zero-stdev bug); proposed per-pose capture | D-17 |
| 2026-10-24 | −1 | H-34 repeated: INF reasoned from CV_8UC1, but the distance image is CV_32S (D-08); INT_MAX + 1 overflows | H-34 |
| 2026-10-24 | +1 | Grassfire pseudocode: backward-pass rule exactly right (`min(curr, min(DOWN,RIGHT)+1)`) and border = bg applied from D-08 without prompting | H-06, D-08 |
| 2026-10-24 | −1 | H-33 repeated: `dest.setTo(INFINITY)` initialises every pixel to INF; bg must start at 0 | H-33 |
| 2026-10-24 | −1 | H-42 repeated (one message after it was given): distance image created as `frame.type()` (CV_8UC1) and INF = float INFINITY | H-42 |
| 2026-10-24 | +1 | Correct 8-conn neighbour sets for both passes; INF = rows + cols; forward pass restricted to fg | H-32, H-42 |
| 2026-10-24 | −1 | H-33 repeated (2nd time): every pixel still set to INF; bg must start at 0 | H-33 |
| 2026-10-24 | −1 | H-45 repeated: local distance Mat put inside grassfire instead of shrink/grow; grassfire's own output became CV_8UC1 | H-45 |
| 2026-10-24 | +1 | shrink comparison fixed to `<= n` by checking it against the test case (awarded in session, logged late at checkpoint 2) | D-05 |
| 2026-10-24 | +1 | Session 2: caught on your own that a browser→server.exe design has no home on Netlify ("where will the camera server be?"), i.e. applied the D-13 WebRTC objection unprompted | D-24 |
| 2026-10-24 | +1 | Proposed running the model in a Web Worker with no server (H-03 applied unprompted, back to your own D-03 plan) when hosting got expensive; it became D-27 and shipped. Pipeline half was missing (probe still open) | D-27 |
| 2026-10-24 | +1 | Live test: noticed the OBB sat on the phone's camera module, not the phone, i.e. the error was segmentation (T1/T3), not classification | T3 |
| 2026-10-24 | +1 | Explained the failure chain from the Threshold/Regions views: hand merged with the phone, touched the border, region rejected, camera ring won (answers the T3 explain-it-back check) | T3, D-08 |
| 2026-10-24 | +1 | Theme review: asked whether the lime box stays visible on the bright paper before it shipped (it would not: led to the halo overlay) | D-28 |

## Progress updates

<!-- One entry per 6h checkpoint: score trend, strengths, recurring gaps, next focus. -->

### Checkpoint 1: 2026-10-24 ~00:00 (session 1, 17:18 → midnight)

**Score: +6** (18 earned, 12 lost). Trend: started 0 → dipped to −1 mid-ISODATA → climbed to +6.

**Strengths**
- Honest about not knowing ("idk", "autofill did it", "I've seen the hint"). That makes mentoring efficient.
- Good scoping instincts: verified an AI claim, cut scope by grading value, built in fallbacks with exit criteria.
- Concepts land once explained: grassfire shapes, do-while semantics, cv::Mat aliasing.

**Recurring gaps**
1. **C++ basics repeated** (8 of 12 losses): const on inputs/outputs, out-params, namespace, headers, return types. These are one-time costs, so they should fade.
2. **Partial answers.** Multi-part questions often get only the first part answered (sampling, empty cluster, border policy, Monday hours took several asks). Answer every Q-number, even with "idk".
3. **Compiler output not read first.** C4244 and C4189 pointed straight at bugs.

**Next focus:** grassfire bodies passing `tests.exe`; read the warnings before asking.


### Checkpoint 2: 2026-10-24 ~18:00 (00:00 → 18:00, ~10 h of active work)

**Score: +8** (25 earned, 17 lost). Trend: +6 → peaked at +10 → dipped to +7 during grassfire → +8.

**Strengths**
- **Product and architecture judgment is now the strongest skill.** You chose the browser-side compute for Netlify, rejected YOLO on cost, proposed per-pose capture (which found the root cause of a real bug), designed the 3 GUI modes, and picked option (a) for the border rule from evidence.
- You question data instead of trusting it: verified captures, accepted the audit's rejects, and kept hard test cases.
- You're honest about where code came from ("got it from Google", "autofill did it"), which kept review accurate.

**Recurring gaps**
1. **Image/data-type bookkeeping in C++** (most of the losses this period): which Mat is input vs output, CV_8U vs CV_32S, and initialising bg/fg (H-33 ×2, H-42, H-45). One rule fixes most of it: *read the input, write the output, and the output's type follows its meaning* (H-49).
2. **Unanswered questions:** the closing-gap numbers, the explain-it-back checks, and the T3/T4/T6 review checks are still open. These are what the 2-point Q&A will probe.
3. **Pseudocode → code jumps:** code got written before the pseudocode round finished, and gaps carried over.

**Next focus:** answer the explain-it-back and review checks (Q&A prep, D-20 condition), then the demo video and report.
