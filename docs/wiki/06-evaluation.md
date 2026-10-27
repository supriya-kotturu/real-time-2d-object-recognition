# 6. Evaluation

## The idea

Testing on the images you trained on measures memory, not recognition. A fair evaluation uses
**images the classifier hasn't seen**, of the same objects in other positions and orientations.

A **confusion matrix** is a K×K table of counts: row = true label, column = predicted label. The
diagonal is correct; off-diagonal cells show *which* objects get mistaken for which, which is far
more useful than one accuracy number. (It's not the data matrix of feature vectors.)

## In this project ([src/apps/eval.cpp](../../src/apps/eval.cpp), D-14)

Three protocols, each run for hand features, ResNet18 cosine and ResNet18 SSD on identical data:

| Protocol | Database | Tested | Hand | Cosine | SSD |
|---|---|---|---|---|---|
| split (T7) | 58 training captures | 47 held-out captures | 26/47 | 47/47 | 45/47 |
| one-shot (T9) | 1 capture per object | 47 held-out | 22/47 | 44/47 | 42/47 |
| leave-one-out | all training captures but one | each training capture | 32/58 | 57/58 | 57/58 |

- 48 test captures; one notebook capture yields no region, so 47 are scored.
- Hard test images were kept on purpose: choosing an easy test set inflates accuracy.
- The task asks for 5×5; with 10 objects the report shows the full 10×10 matrix.
- Live evaluation: Evaluate mode in the GUI, where you set the true label and record each
  prediction (the task's "hook to tell the system the true label").

Reading the hand-feature matrix: distinct silhouettes are fine (sticky-notes 5/5, toy-car 4/4,
keychain/notebook/spects 4/5, mouse 3/4); blob-like objects fail (ball 0/6 → mostly
sticky-notes, batteries 0/4, cloud 1/5 → mostly mouse, kitchen-towel 1/4).

## Check yourself

<details><summary>Why is leave-one-out not enough on its own?</summary>

It tests on training *captures*: they were taken in the same session and lighting as their
neighbours, so a near-duplicate is often still in the database. Separate test captures are the
honest number.
</details>

<details><summary>What does "one-shot" add?</summary>

It shows how each method does with a single example per object, which is the realistic case for
teaching a new object. ResNet18 barely drops (47 → 44); hand features drop more (26 → 22).
</details>

<details><summary>Why did adding 5 extra objects hurt the hand features?</summary>

Seven of the ten objects are blob-like (hu1 between 0.17 and 0.20), so in shape-feature space they
crowd together. More objects means more neighbours close to each one.
</details>
