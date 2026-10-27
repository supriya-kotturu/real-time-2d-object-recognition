# 5. Classification (nearest neighbour)

## The idea

**Nearest neighbour (NN):** compare the new feature vector with every stored sample and take the
label of the closest one. No training step beyond storing examples, which is why one Train press
teaches a new object.

Features have different ranges (hu3 ~ 0.0001, aspect ratio ~ 0.5), so plain Euclidean distance
would be dominated by the large ones. **Scaled Euclidean** divides each difference by that
feature's standard deviation σ over the database, so each feature counts in "how unusual is this
difference for this feature" units.

**Closed set:** NN can only answer with a label it has stored. To say "I don't know this", you
need a **cutoff**: if even the nearest sample is too far, answer "unknown".

## In this project ([src/core/classifier.cpp](../../src/core/classifier.cpp))

- d = sqrt(Σ ((x_i − y_i) / σ_i)²). (The task writes it without the square root; the nearest
  neighbour is the same either way.)
- **σ floor:** σ_i = max(stdev, 1% of |mean|, 1e-9).
- **Unknown:** distance above a cutoff → "unknown". Cutoffs per metric, because the scales
  differ: scaled Euclidean 3.0, cosine 0.35, SSD 500 (from the largest distances of correct
  matches in eval).
- Fewer than 2 samples → nothing can be classified; every object counts as unknown, which is
  what lets auto-learn start from an empty database.
- Database: CSV, one row per sample, `label,f1..f5`. A separate CSV holds ResNet18 embeddings,
  because embeddings from different models live in different spaces (H-01).

## Check yourself (explain-it-back check)

<details><summary>Why floor σ instead of skipping a feature whose σ is 0?</summary>

σ ≈ 0 happens when the database has one class or near-identical samples. Dividing by it explodes
the distance; skipping it seems safe, but if *every* feature is skipped every distance becomes 0
and any object matches anything. A floor keeps the feature in play without letting noise dominate.
This was a real bug: auto-learn's burst of identical frames gave σ ≈ 0.
</details>

<details><summary>The phone isn't in the database. What can NN possibly answer?</summary>

Only one of the 10 stored labels, or "unknown" if the nearest distance exceeds the cutoff. It
can't invent "phone". That's why the phone came out as ball/cloud, flickering at the cutoff.
</details>

<details><summary>Why one cutoff per metric?</summary>

Scaled Euclidean distances here run roughly 0.05–6, cosine distances 0–2 (in practice 0.03–0.4),
SSD into the hundreds. A single number can't be a sensible "too far" for all three.
</details>

<details><summary>What's the trade-off when lowering the cutoff?</summary>

More unfamiliar objects become "unknown" (good), but more real objects at an unusual pose or
lighting also become "unknown" (false unknowns). One-shot correct cosine matches reach 0.36, so
0.35 is already tight.
</details>
