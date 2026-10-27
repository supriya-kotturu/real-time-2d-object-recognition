# 4. Moments and features

## The idea

**Moments** summarise a region's shape with sums over its pixels:

- m00 = area; centroid x̄ = m10/m00, ȳ = m01/m00.
- **Central moments** are measured from the centroid, so they ignore *where* the object is:
  μ20 = Σ(x − x̄)², μ02 = Σ(y − ȳ)², μ11 = Σ(x − x̄)(y − ȳ).
  μ20 and μ02 are the spread along x and y; **μ11 says whether x and y grow together**, i.e.
  whether the object is tilted.
- **Primary axis** (axis of least central moment): θ = ½·atan2(2μ11, μ20 − μ02). The direction
  the object is longest along.
- **Normalised central moments** η = μ / m00^(1 + (p+q)/2) also remove scale.
- **Hu invariants** combine η's so rotation drops out too.

A feature is useful for recognition when it's **invariant** to translation, scale and rotation
(the object can be anywhere, any size, any angle) but **different between objects**.

## In this project ([src/core/features.cpp](../../src/core/features.cpp))

- Moments computed from scratch over the region's pixels (not `cv::moments`).
- **Oriented bounding box (OBB):** project every pixel onto the primary axis and its
  perpendicular; the min and max along each give the box. Lime in the overlay, axis in blue.
- Five features:
  - **percentFilled** = region area / OBB area
  - **aspectRatio** = OBB short side / long side (in (0, 1])
  - **hu1** = η20 + η02: spread of mass around the centroid
  - **hu2** = (η20 − η02)² + 4η11²: elongation
  - **hu3**: third-order moments, asymmetry
- Report images: toy-car 0.797, 0.485, 0.221, 0.0208, 0.000196; spects 0.407, 0.339, 0.643,
  0.285, 0.000326; ball 0.730, 0.867, 0.162, 0.000447, 0.0000586.
- **Measured invariance** (extension, [src/apps/invariance.cpp](../../src/apps/invariance.cpp)):
  each of the 48 test objects rotated 0–345° and scaled 0.5–1.5×. Spread (stdev / mean): fill,
  aspect and hu1 under 1%; hu2 2–3%; hu3 3.5–5% (higher-order moments are more sensitive to
  pixel-level noise at the outline).

## Check yourself (explain-it-back checks)

<details><summary>What does μ11 measure, and what is it for a shape aligned with the axes?</summary>

The covariance of x and y over the region. For a shape symmetric about the x or y axis it's 0;
for a shape tilted up-right it's positive, down-right negative. It's what makes θ non-zero.
</details>

<details><summary>Why build the OBB from the extents along θ instead of using cv::minAreaRect?</summary>

minAreaRect finds the smallest-area box, whose angle can disagree with the primary axis and can
jump between frames for nearly square shapes. Building the box along θ makes the box, the axis and
the features consistent with each other, and it's what the task asks for.
</details>

<details><summary>Why are sticky-notes and the ball hard for these features?</summary>

A square and a disc have μ20 ≈ μ02 and μ11 ≈ 0, so θ is undefined: noise decides it. The box
tilts arbitrarily (visible in the T6 figure), and their aspect ratios and hu1 come out almost the
same.
</details>

<details><summary>Which features are invariant to what?</summary>

All five are translation-invariant (central moments, a box relative to the object),
scale-invariant (ratios, normalised moments) and rotation-invariant (box along the object's own
axis; Hu combinations). The measured drift confirms it for fill, aspect and hu1.
</details>
