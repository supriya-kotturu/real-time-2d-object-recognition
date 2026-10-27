# 3. Regions (connected components)

## The idea

Connected-components labelling gives every separate blob of foreground its own number, so the
system can talk about "this object" instead of "all white pixels". It also returns per-region
statistics: area, bounding box, centroid.

The hard part isn't labelling; it's **choosing which region is the object** when there are several
(specks that survived clean-up, a shadow, a bit of table at the edge).

## In this project ([src/core/regions.cpp](../../src/core/regions.cpp))

- OpenCV `connectedComponentsWithStats` on the cleaned image (not from scratch, D-09: the time
  went into other parts of the project instead).
- Regions smaller than **0.2% of the frame** are dropped (scales with resolution).
- The **5 largest** are kept.
- **Primary region = the one whose centroid is closest to the image centre, among regions that
  don't touch the border.** Returns none if every region touches the border.
- In the report's toy-car image a second region (a dark strip along the top edge) touches the
  border and is excluded.

## Test your understanding

<details><summary>Why "most central and not touching the border" instead of "the largest region"?</summary>

The largest region is often wrong: a shadow, a dark table edge or an arm can be bigger than the
object. A region touching the border is probably cut off, so its area, moments and box would be
wrong and give a confident wrong answer. The user naturally puts the object in the middle, so
"most central, fully visible" matches intent.
</details>

<details><summary>What did the border rule do in the phone test?</summary>

A hand holding the phone merged with it into one region that touched the border, so the whole hand+phone
region was rejected. The camera module, a dark disc inside a lighter ring and fully inside the
frame, became the primary region, so the classifier saw a disc and said "ball".
</details>

<details><summary>Why drop small regions by a fraction of the frame instead of a fixed pixel count?</summary>

So the rule behaves the same at 640×480 and at a phone's higher resolution: 0.2% of the frame is
a speck at any size.
</details>
