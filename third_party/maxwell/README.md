# Instructor utilities (reference only)

`utilities.cpp` is Bruce A. Maxwell's CS5330 course code for Task 9 (ROI extraction and ResNet18
embedding), kept **unmodified** for attribution. It is **not compiled**: it includes the course's
`features.h`/`vision.h`, which are not part of this project.

Our implementation, `include/app/embedding.h` / `src/app/embedding.cpp`, follows the same approach
but is written for this project's `RegionFeatures` and OpenCV 5 (decision D-18). Differences:

| Instructor code | Ours | Why |
| --- | --- | --- |
| `getRotationMatrix2D(c, -theta, 1)` | `+theta` | our theta is measured in image coordinates (y down) |
| debug `cv::rectangle` drawn before the crop | no drawing on the source | the 4 px gray frame leaked into every embedding |
| `M_PI` | `CV_PI` | `M_PI` needs `_USE_MATH_DEFINES` on MSVC |
| 180° ambiguity of the primary axis ignored | flipped so the third moment along the axis is positive | the same object rotated 180° gives the same ROI |
| comment: embed the thresholded image | embeds the original image (Task 9, step C) | the colour and texture information is the point of the embedding |
