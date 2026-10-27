# 7. Embeddings (ResNet18, Task 9)

## The idea

A CNN trained on ImageNet turns an image into a vector (an **embedding**) where similar-looking
things land close together. Take the output of the **global-average-pooling** layer (just before
the classification layer) and you get a general-purpose description of "what this looks like".

**One-shot classification:** store one embedding per object; classify a new image by its nearest
stored embedding. No retraining, ever.

**ResNet18 is not rotation-invariant.** A rotated object gives a different embedding. Rotation
has to be removed *before* the network sees the image.

## In this project ([src/app/roi.cpp](../../src/app/roi.cpp), [src/app/embedding.cpp](../../src/app/embedding.cpp))

Steps A–E, after the instructor's utilities, ported to OpenCV 5:

- **A.** From T4: centroid, θ, extents along both axes.
- **B.** Rotate the **original colour frame** by θ about the centroid, so the primary axis lies
  along x. An axis has no direction (θ and θ + 180° are the same line), so the sign of the
  **third moment** along the axis decides which end goes left: an upside-down object gives the
  same crop.
- **C.** Crop the now axis-aligned box (the ROI, region of interest).
- **D.** Resize to 224×224; ImageNet normalisation: means (124, 116, 104), stdev 0.226.
- **E.** ResNet18 v2 (ONNX model zoo); embedding = 512-d output of the pooling layer.

Classification: cosine distance (1 − cos) by default, SSD as an alternative.

- **Cosine beats SSD** (47 vs 45 on the split): cosine ignores the vector's length, which changes
  with image contrast; SSD doesn't.
- **Why it beats hand features:** it sees colour and texture. An orange ball and a pink square of
  sticky notes have nearly the same shape features but very different embeddings.
- **Limitation:** exposure. A uniform +10 grey-level brightening turned the hardest toy-car test
  image (cosine 0.26) into keychain; thresholded shape features barely change.
- **In the browser:** onnxruntime-web only returns a model's *declared* outputs, and the pooling
  output is internal. The worker adds it as a declared output by splicing 46 bytes into the
  model's protobuf (weights untouched), after checking the download's SHA-256.

**YOLO instead?** YOLO is a **detector** trained on 80 fixed COCO classes. Its backbone *can*
give an embedding too, but it's trained to locate and box objects, not to summarise them.
Teaching it *new* objects means retraining with hand-drawn boxes, which is the opposite of
one-shot. (Lighter than ResNet18, not heavier: YOLOv8n is ~3 M parameters vs 11.7 M.)

## Test your understanding

<details><summary>What has to happen to a frame before ResNet18 can embed it?</summary>

The whole classical front end: threshold, clean-up, regions, moments. You need the primary region,
its centroid, θ and extents to rotate and crop the ROI. ResNet18 only sees the aligned crop.
</details>

<details><summary>Where does rotation invariance come from in the embedding path?</summary>

From step B (rotating by θ and resolving 180° with the third moment), i.e. from the moments, not
from the network.
</details>

<details><summary>Why one embedding database per model?</summary>

Embeddings from different models live in different spaces: dimension 512 vs 1280 is the obvious
part, but even two 512-d models aren't comparable. Mixing them makes distances meaningless.
</details>

<details><summary>Why did the embedding survive one-shot so well?</summary>

The network already learned general visual features on ImageNet; one example is enough to place
an object in that space. Hand features have only 5 numbers, and several objects share them.
</details>
