# Silhouette wiki: the ideas behind the website

[Silhouette](https://real-time-object-recognition.netlify.app) recognises objects lying on a
plain background from a camera, entirely in your browser. These pages explain the computer
vision and engineering that make it work, from turning a camera frame into a black-and-white
silhouette to running a C++ pipeline and a neural network inside a web page.

Each page has three parts:

1. **The idea**: the concept in plain terms.
2. **In this project**: how Silhouette applies it, with the real parameters, results and source files.
3. **Test your understanding**: a few questions, with answers you can expand.

| # | Topic | What you'll learn |
|---|---|---|
| 1 | [Thresholding (ISODATA)](01-thresholding.md) | separating an object from the background with k-means on brightness |
| 2 | [Morphology and grassfire](02-morphology-grassfire.md) | cleaning a binary image; distance transforms in two passes |
| 3 | [Regions](03-regions.md) | labelling connected blobs and choosing which one is the object |
| 4 | [Moments and features](04-moments-features.md) | describing a shape so it doesn't change with position, size or rotation |
| 5 | [Classification](05-classification.md) | nearest neighbour, scaled distances, recognising "unknown" |
| 6 | [Evaluation](06-evaluation.md) | honest test sets and reading a confusion matrix |
| 7 | [Embeddings (ResNet18)](07-embeddings.md) | one-shot recognition with a pretrained network; why not YOLO |
| 8 | [C++ notes](08-cpp-concepts.md) | the C++ and OpenCV details that matter in image code |
| 9 | [System and web](09-system-web.md) | one pipeline in three runtimes, WebAssembly, WebGPU, flow control |
| 10 | [Failure analysis](10-failure-analysis.md) | where it breaks, and why |

For the overall design with diagrams, see [ARCHITECTURE.md](../../ARCHITECTURE.md). Every
design choice is recorded with its alternatives in the
[decision log](../mentorship/decisions.md) (referenced below as D-xx).

Silhouette was built for CS5330 (Pattern Recognition and Computer Vision) at Northeastern
University.
