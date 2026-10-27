# Silhouette wiki: revision notes for the Q&A

The 2-point Q&A asks you to explain your implementation and extensions. Each page below covers
one topic in three parts:

1. **The idea**, in plain terms.
2. **In this project**: the actual numbers, files and decisions you can quote.
3. **Check yourself**: questions to answer out loud before opening the hidden answer. If you
   can't answer one, reread the page; that's the point of it.

| # | Page | Revise if you can't explain… |
|---|---|---|
| 1 | [Thresholding (ISODATA)](01-thresholding.md) | why k-means with K = 2, why sample 1/16 of the pixels |
| 2 | [Morphology and grassfire](02-morphology-grassfire.md) | the two passes, closing vs opening, the border rule |
| 3 | [Regions](03-regions.md) | why "central and off-border" rather than "largest" |
| 4 | [Moments and features](04-moments-features.md) | μ11, the primary axis, the oriented box, Hu invariants |
| 5 | [Classification](05-classification.md) | scaled Euclidean, the stdev floor, unknown objects |
| 6 | [Evaluation](06-evaluation.md) | the three protocols and how to read the confusion matrix |
| 7 | [Embeddings (ResNet18)](07-embeddings.md) | alignment steps A–E, cosine vs SSD, YOLO vs ResNet18 |
| 8 | [C++ concepts](08-cpp-concepts.md) | cv::Mat sharing, out-parameters, const references, CV_32S |
| 9 | [System and web](09-system-web.md) | Session, flow control, WebAssembly, the worker, WebGPU |
| 10 | [Failure analysis](10-failure-analysis.md) | why the phone was "ball", why blobs get confused |
| 11 | [Likely questions](11-likely-questions.md) | a 20-question drill, mixed topics |

**Suggested order with little time:** 11 first (drill), then whatever you missed.

**Be ready to say who wrote what.** Task 1 you wrote by hand. Task 2's grassfire was written by
the AI from your pseudocode, and you hand-checked small grids. The rest was written by the AI
from your design decisions (see [decisions.md](../mentorship/decisions.md)). Graders accept AI
help that's declared; what they test is whether you understand it.
