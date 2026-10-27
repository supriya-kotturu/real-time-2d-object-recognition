# 11. Likely questions (drill)

Answer each out loud in two or three sentences before opening it.

<details><summary>1. Which tasks did you write from scratch, and which used OpenCV or AI help?</summary>

Task 1 (ISODATA + applying the threshold) by hand. Task 2's grassfire grow/shrink was written by the
AI from my pseudocode, and I hand-checked it. Moments and features are computed without
cv::moments. T3 uses OpenCV connected components. The rest was AI-written from my design decisions.
</details>

<details><summary>2. Explain ISODATA in one breath.</summary>

k-means with two clusters on pixel intensities: start with a dark and a light mean, assign each
sampled pixel to the nearer one, recompute the means, repeat until they move less than 0.5 (max 15
times), threshold at the midpoint.
</details>

<details><summary>3. Why the saturation step?</summary>

Coloured objects (orange ball, pink towel) are about as bright as white paper. Value × (1 −
saturation) darkens saturated pixels but leaves grey shadows bright.
</details>

<details><summary>4. Closing vs opening, and your order?</summary>

Closing (grow then shrink) fills holes and gaps; opening (shrink then grow) removes specks. Closing
first (n = 2), then opening (n = 1), so no shrink widens a gap before it's filled.
</details>

<details><summary>5. How does grassfire compute distances in two passes?</summary>

Forward pass top-left to bottom-right: distance = 1 + min of already-visited neighbours (up, left,
and for 8-connectivity up-left, up-right). Backward pass from the bottom-right with down, right
(and down-left, down-right), keeping the min.
</details>

<details><summary>6. How do you pick the object among several regions?</summary>

Drop regions under 0.2% of the frame, keep the 5 largest, then pick the one nearest the image
centre that doesn't touch the border.
</details>

<details><summary>7. What's the primary axis and how is it computed?</summary>

The direction of least central moment, where the object is longest: θ = ½·atan2(2μ11, μ20 − μ02).
</details>

<details><summary>8. How is the oriented bounding box built?</summary>

Project every region pixel onto the primary axis and its perpendicular; min and max along each give
the box. Not minAreaRect, so it always agrees with θ.
</details>

<details><summary>9. Your five features and why they're invariant?</summary>

percentFilled, aspectRatio, hu1, hu2, hu3: ratios and normalised central moments (scale), central
moments (translation), box along the object's own axis and Hu combinations (rotation). Measured
drift under 1% for the first three.
</details>

<details><summary>10. What is scaled Euclidean and why scale?</summary>

Divide each feature difference by that feature's stdev over the database, so a feature with a big
numeric range can't dominate one with a small range.
</details>

<details><summary>11. Why floor the stdev?</summary>

Near-zero stdev (one class, identical samples) would explode the distance; skipping it can make all
distances 0. A floor of 1% of the mean keeps the feature without letting noise dominate.
</details>

<details><summary>12. How do you detect unknown objects?</summary>

If the nearest distance is above a cutoff (3.0 scaled Euclidean, 0.35 cosine, 500 SSD), the
answer is "unknown". Cutoffs come from the largest distances of correct matches.
</details>

<details><summary>13. How did you evaluate, and what were the results?</summary>

Separate test captures: 58 training, 47 scored test images. Hand features 26/47 (55%), ResNet18
cosine 47/47, SSD 45/47. One-shot: 22 vs 44 vs 42. Leave-one-out: 32/58 vs 57/58.
</details>

<details><summary>14. Which objects failed with hand features and why?</summary>

Ball, batteries, cloud, towel: blob-like, no defined primary axis, similar aspect and hu1. Ball was
mostly called sticky-notes (disc vs square look alike in these features).
</details>

<details><summary>15. How does the ResNet18 path work?</summary>

Rotate the original frame by θ (180° fixed with the third moment), crop the oriented box, 224×224,
ImageNet-normalise, take the 512-d pooling output, cosine nearest neighbour against one or more
stored embeddings.
</details>

<details><summary>16. Why does ResNet18 do so much better?</summary>

It sees colour and texture, which shape features ignore. It's limited by exposure sensitivity (one
toy-car flipped to keychain after brightening).
</details>

<details><summary>17. Why ResNet18 and not YOLO?</summary>

YOLO is a detector over 80 fixed classes; teaching it new objects means retraining with boxes.
ResNet18 + nearest neighbour learns a new object from one example. (YOLO isn't heavier; its
backbone could give embeddings, but they're trained for localisation.)
</details>

<details><summary>18. How does auto-learn work?</summary>

An object that stays unknown and still for 3 s triggers a "name this" prompt. Then 5 samples, one
per pose: a new pose needs a 10%-of-diagonal move or 20° rotation, then 0.6 s still. That avoids
5 identical frames, which taught nothing and gave stdev ≈ 0.
</details>

<details><summary>19. How does it run in the browser without a server?</summary>

The same C++ compiled to WebAssembly, in a Web Worker; ResNet18 in onnxruntime-web on WebGPU;
samples stored in IndexedDB. Netlify only serves static files. Same results as the native build.
</details>

<details><summary>20. What would you improve with more time?</summary>

Handle hand-held objects (a hand detector or a skin mask, tested against pink/orange objects first),
exposure normalisation for the embedding, more poses for articulated objects, and comparing another
embedder (e.g. a YOLOX backbone) in eval.
</details>
