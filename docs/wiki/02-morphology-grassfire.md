# 2. Morphology and the grassfire transform

## The idea

The binary image has two kinds of defects: **specks** (small false foreground) and **holes or
breaks** inside the object (glare, printed text). Morphology fixes them with two primitives:

- **Shrink (erode) by n:** remove foreground pixels within n of the background.
- **Grow (dilate) by n:** add background pixels within n of the foreground.

Combined:

- **Opening = shrink then grow:** removes specks and thin spurs; big shapes come back the same size.
- **Closing = grow then shrink:** fills holes and narrow gaps; the outline comes back the same size.

**Grassfire** computes, for every foreground pixel, its distance to the nearest background pixel,
in two raster passes instead of a search per pixel:

- **Forward pass** (top-left → bottom-right): d = min(already-visited neighbours) + 1.
  4-connected: up, left. 8-connected also: up-left, up-right.
- **Backward pass** (bottom-right → top-left): d = min(d, min(down, right [, down-left, down-right]) + 1).

4-connectivity gives city-block distance (diamond shapes); 8-connectivity gives chessboard
distance (squares).

## In this project ([src/core/morphology.cpp](../../src/core/morphology.cpp))

- Grow and shrink are implemented from scratch, with tests on small hand-computed grids.
- Order: **closing (grow 2, shrink 2), then opening (shrink 1, grow 1)**, 8-connected (D-21).
  Closing first, so no shrink widens a gap before it's filled.
- Distance image is **CV_32S** (int), not CV_8U: distances can exceed 255 and "infinity + 1"
  must not wrap. INF = rows + cols, larger than any real distance.
- Initialisation inside the forward loop: background → 0, foreground → INF.
- Shrink by n: foreground pixels with distance ≤ n become background.
- Grow by n: run grassfire on the **inverted** image (distance from each background pixel to the
  nearest foreground); background pixels with distance ≤ n become foreground.
- Border rule: in grow and shrink, outside the image counts as **far away** (D-08). An object
  touching the edge stays touching it, so the region step can still reject it.
- Limit: a 6–8 px glare gap on a metal blade needs n ≥ 4, which also merges nearby parts and
  rounds corners, so n = 2 was kept.

## Test your understanding

<details><summary>Why does grow treat outside the image as "far", not as foreground?</summary>

Grow runs on the inverted image. If outside counted as background of the inverted image (= the
original's foreground), every background pixel within n of the frame edge would turn into
foreground: a white band would grow in from all four edges.
</details>

<details><summary>Why can the background/foreground initialisation live inside the forward-pass loop?</summary>

The forward pass visits pixels in raster order and only reads neighbours above and to the left,
which it has already initialised and updated. So each pixel can be set (0 or INF, then min + 1)
the moment it's reached.
</details>

<details><summary>Why does the 8-connected forward pass never read the down-left neighbour?</summary>

Down-left lies in the next row, which the forward pass hasn't reached yet, so its value isn't
final. Down-left belongs to the backward pass, which comes from the bottom-right.
</details>

<details><summary>Why closing before opening, not the other way round?</summary>

Opening first would shrink: a thin bridge across a glare gap, or a narrow part, could vanish
before the closing had a chance to fill the gap. Closing first seals the object; the opening then
removes the specks, including any the closing joined to the object.
</details>

<details><summary>Why is the distance image CV_32S and not CV_8UC1?</summary>

With border = background the worst distance is about min(W, H)/2 (240 at 640×480, 360 at 720p),
which overflows uchar's 255. Also INF + 1 on a uchar would wrap to 0. int has room and simple maths.
</details>
