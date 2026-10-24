# Project 3: Real-time 2-D Object Recognition

Source: `task.pdf` (Canvas assignment). Points: 30 (2 of which are the post-grading Q&A). Language: **C++ with OpenCV** (Python allowed only for offline eigenspace building, Task 9).

## Goal

Recognize single dark objects on a uniform light background from a top-down camera, invariant to **translation, scale, and rotation**. Output an image/video showing each object's position and category. Real-time on video if possible; otherwise process a directory of images or a pre-recorded video (the report must say which).

## Constraints

| Constraint | Detail |
| --- | --- |
| Objects | ≥ 5, distinguishable by 2D shape, mostly uniform dark color |
| Scene | Single object required; multiple objects = extension |
| From-scratch rule | Solo: write **≥ 1** of tasks 1–4 from scratch (threshold, morphology, connected components, moments). Pairs: ≥ 2. Others may use OpenCV |
| Features | **Region-based**, not boundary-based. Must be T/S/R invariant |
| Dev data | `data/examples/*.png` (course dev image set) |
| Input options | Webcam facing down on white paper · laptop cam vs white wall · phone stream · still images / recorded video |

## Pipeline

```
frame → [1] threshold → [2] morphology → [3] connected components → [4] features
      → [5] train (store to DB) | [6] classify (nearest neighbor) → overlay label
      → [9] embedding path (rotate/crop ROI → ResNet18 or eigenspace) → one-shot classify
```

## Tasks

Each task lists **Implement** (code), **Display** (runtime visualization), and **Report** (deliverable). Report images for tasks 1–4 must use the **same 2–3 source images**.

### 1. Threshold the input video
- **Implement:** separate dark object from light background.
  - Optional pre-processing: slight blur; darken high-saturation pixels (background is unsaturated); custom threshold using saturation + intensity.
  - Optional dynamic threshold: ISODATA — k-means K=2 on a random sample (~1/16 of pixels), threshold at midpoint of the two means.
- **Display:** thresholded video in its own window.
- **Report:** 2–3 thresholded examples.

### 2. Clean up the binary image
- **Implement:** morphological filtering (grow/shrink, opening/closing) chosen to fix observed issues (noise, holes). Median filter allowed in addition.
- **Report:** 2–3 cleaned images (same images as Task 1) + reasoning for the chosen strategy.

### 3. Segment into regions
- **Implement:** connected components on the cleaned image.
  - If using OpenCV: `connectedComponentsWithStats`. If from scratch: two-pass or region growing.
  - Ignore regions below a size threshold; optionally keep the largest N (N ≥ 1).
  - Starting heuristic: pick the region that is large enough, most central, and doesn't touch the image border (uses area, centroid, axis-aligned bbox).
  - Renumber surviving regions sequentially.
- **Display:** region map colored from a palette (e.g. 256 random colors). Optional: match centroids frame-to-frame to keep colors stable (avoid flicker).
- **Report:** 2–3 colored region maps (same images).

### 4. Compute features for each major region
- **Implement:** `features(regionMap, regionId)` →
  - **Required:** axis of least central moment; oriented bounding box aligned to that axis (not `minAreaRect`, which may disagree and be less stable).
  - Suggested first features: percent filled, bbox height/width ratio. Add more (e.g. central moments / Hu moments). Start with 2–3.
  - If using `cv::moments`, the report must explain which moments it computes and which are used.
- **Display:** primary axis + oriented bbox overlaid, rotating with the object; ≥ 1 feature value rendered live (to sanity-check invariance by moving/rotating the object).
- **Report:** 2–3 images with axis + OBB (same images); feature vector for each; explanation of each feature.

### 5. Collect training data
- **Implement:** training mode storing labeled feature vectors to an object DB (file, e.g. CSV).
  - Suggested: key press `N` → prompt for label → append current object's feature vector + label.
  - Alternative: batch from labeled still images.
- **Report:** explain how training works.

### 6. Classify new images
- **Implement:** nearest-neighbor against DB using **scaled Euclidean distance**: `Σ ((x1 − x2) / stdev_x)²`, stdev per feature over the DB. Other metrics optional.
- **Display:** predicted label on the output stream.
- **Extension:** detect unknown objects (distance threshold).
- **Report:** one result image per category with label clearly shown.

### 7. Evaluate performance
- **Implement:** run on ≥ 3 images per object in different positions/orientations. Hook to tell the system the true label to speed up tallying.
- **Report:** 5×5 confusion matrix (true vs. predicted).

### 8. Capture a demo
- Video of the system classifying objects (classical, embedding, or both). If still-image system, show the visualizations.
- **Report + readme:** link to the video.

### 9. One-shot classification with image embeddings
Pre-processing (assumes tasks 1–4 done on the primary region). Instructor utilities code implements A–C plus ResNet18 embedding — usable **with attribution** (download links in `task.pdf`, p.4).
- **A.** Have centroid `(cx, cy)`, primary axis angle θ, and min/max extent along primary and secondary axes.
- **B.** Rotate the **original image** by −θ so the primary axis aligns with X.
- **C.** Crop ROI from the rotated original at the now axis-aligned bbox.
- **D.** Resize to square (224×224 for CNN, 64×64 for PCA).
- **E.** Embed:
  - **CNN option:** pre-trained ResNet18, take output of global average pooling (next-to-last layer).
  - **Eigenspace option:** build from pre-processed training images (Python allowed offline, instructor code available); store mean image + eigenvectors; project in C++ at runtime.
- Classify with SSD or cosine distance against stored training embeddings (one example per class suffices).
- **Report:** how the embedding was built; performance comparison vs. hand-built features (Task 7).

## Extensions (optional, for extra credit)
- Better GUI for running the system and managing DB entries.
- Write more than the required number of stages (tasks 1–4) from scratch.
- ≥ 5 additional objects in the DB (1 point) + discuss impact.
- Auto-learn: detect unknown object, then collect its stats; demo building a DB quickly.
- Experiment with other features, classifiers, embeddings, or distance metrics.
- Explore OpenCV object recognition tools.
- 2-D plot of embeddings (project onto top 2 eigenvectors); assess class separation.
- Multiple objects per image (Task 3 note).
- Unknown-object detection (Task 6 note).

## Report (PDF, no code)
1. Project description, own words, ≤ 200 words. State real-time vs. image/video-directory system.
2. Required images, each with a short description.
3. Extensions: description + example images.
4. Short reflection on what was learned.
5. Acknowledgements (materials, people, instructor utilities code if used).

### Required-images checklist
- [ ] T1: 2–3 thresholded
- [ ] T2: 2–3 cleaned (same images)
- [ ] T3: 2–3 colored region maps (same images)
- [ ] T4: 2–3 with axis + OBB (same images) + feature vectors
- [ ] T6: one labeled result per category
- [ ] T7: 5×5 confusion matrix
- [ ] T8: demo video link
- [ ] T9: embedding method + comparison vs. baseline

## Submission (Gradescope)
- `.cpp` and `.h`/`.hpp` files, PDF report, `readme.txt`/`readme.md`.
- Readme contains: name(s), video links, time travel days used.
- Deadline: full credit incl. extensions (30/30). Up to 1 week late: base only (max 26/30). Time travel days: 8 per semester, ≤ 3 per assignment, whole days only.
- Post-grading: 10-min Q&A with TA/professor = 2 points. Be ready to explain implementation and extensions.
