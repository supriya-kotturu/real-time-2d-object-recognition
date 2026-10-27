# 1. Thresholding (ISODATA)

## The idea

Thresholding turns a grey image into a binary one: object (255) vs background (0). A fixed
threshold breaks as soon as the lighting changes, so the threshold is computed from each frame.

ISODATA is **k-means with K = 2 on pixel intensities**:

1. Start with two means, a dark one and a light one.
2. Assign every pixel to the nearer mean.
3. Recompute each mean from its pixels.
4. Repeat until the means stop moving.
5. Threshold t = midpoint of the two means.

It adapts to *global* lighting (the whole frame darker or brighter), but it's still one
threshold per image, so a *local* shadow can still break it.

**Why not the histogram valley?** Valley-finding looks for the lowest point between the two
histogram peaks. Real histograms are noisy and have many small dips, so the "valley" jumps
around. k-means averages over all pixels, so it's stable.

## In this project ([src/core/threshold.cpp](../../src/core/threshold.cpp))

- **Pre-processing (D-22): saturation darkening**, value × (1 − saturation). White paper and
  grey shadows have saturation ≈ 0 and keep their brightness; coloured objects (orange ball, pink
  towel) become dark. Without it, those objects were only segmented by their dark text or shadow.
- **Initial means:** D = 0, L = 127.5.
- **Sampling:** every 4th row and column, so 1/16 of the pixels. Two means don't need every
  pixel; it keeps this cheap at 30 fps. Only *applying* the threshold visits every pixel.
- **Running sums:** sumD, countD, sumL, countL, reset every iteration. No sets of pixels are
  stored, so there's no allocation per iteration.
- **Stop:** continue while either mean moved more than 0.5 **and** fewer than 15 iterations ran.
  Never compare two doubles with `==`.
- **Empty cluster** (e.g. blank paper): keep the previous mean instead of dividing by zero.
- **Apply:** pixel > t → 0 (paper), else 255 (object). The object is dark, so "≤ t" is foreground.
- Thresholds on the report images: toy-car 113, spects 101, ball 120.

## Test your understanding

<details><summary>Why is ISODATA "k-means with K = 2"?</summary>

Both alternate "assign each point to the nearest centre" and "move each centre to the mean of its
points". Here the points are scalar intensities and there are two centres, dark and light.
</details>

<details><summary>Why sample only 1/16 of the pixels, and why must applyThreshold still visit every pixel?</summary>

Sampling only *estimates* the two means; 1/16 of a 640×480 frame is still ~19,000 pixels, plenty
for two averages. Applying the threshold produces the output image, and `create()` doesn't clear
memory, so any skipped pixel would hold garbage.
</details>

<details><summary>What does saturation darkening fix, and why doesn't it darken shadows?</summary>

Coloured objects on white paper have similar *brightness* to the paper, so plain grayscale can't
separate them. Multiplying by (1 − saturation) darkens saturated pixels. Shadows are grey
(saturation ≈ 0), so they keep their brightness and don't become foreground.
</details>

<details><summary>What would happen with the stop condition `D == prevD && L == prevL`?</summary>

Floating-point means may never be exactly equal again; they can oscillate by tiny amounts. The loop
would run to the iteration cap every time (or forever without a cap). Use |Δ| < ε.
</details>

<details><summary>When does ISODATA fail?</summary>

When one threshold can't separate the scene: a strong local shadow, a very uneven light, or a
cluttered background (the face test: hair, clothes and furniture all become foreground).
</details>
