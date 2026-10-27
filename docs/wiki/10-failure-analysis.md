# 10. Failure analysis

Where Silhouette breaks, and why. Each case traces a wrong answer back to the stage that caused it.

| Case | What happened | Why | Fix / mitigation |
|---|---|---|---|
| Phone → "ball"/"cloud" | Box drawn around the camera module, label flickered | Hand (made dark by saturation darkening) merged with the phone; that region touched the border and was rejected; the camera ring became the primary region; it's a dark disc, and its distance sat at the cosine cutoff | Object on the paper, hands out of frame (the task's assumption); teach "phone" with one Train press |
| Face not trainable | Train stayed disabled | Head/shoulders touch the border; ISODATA splits the whole room by brightness | Out of scope: needs a detector |
| Ball, batteries, cloud, towel (hand features) | 0–1 correct each | Blob-like shapes: no defined primary axis, nearly equal aspect ratio and hu1 | ResNet18 (sees colour/texture): all correct |
| Toy-car → keychain (ResNet18) | One test image flipped | +10 grey-level brightening; it was already the hardest match (cosine 0.26) | Known limitation: exposure sensitivity |
| Metal blade glare gap | Gap not sealed | 6–8 px gap needs n ≥ 4 | Kept n = 2: larger n merges parts |
| Keychain ring, spectacle legs | Silhouette changes with pose | Articulated objects | More training poses |
| One notebook test image | No region | Not investigated (no central off-border region survives) | Skipped and reported (47 of 48 scored) |

## Test your understanding

<details><summary>Walk through the phone failure from camera to label.</summary>

Saturation darkening makes skin dark → ISODATA marks hand + phone as one foreground blob → it
touches the border → T3 drops it → the camera module (dark disc, fully inside) is the most central
off-border region → features/embedding of a disc → nearest stored round objects (ball, cloud) at a
distance right at the cutoff → flickers between ball, cloud and unknown.
</details>

<details><summary>Is the classifier "wrong" in the phone case?</summary>

No: it answered correctly for what it was given (a disc), and it can only answer with stored
labels. The failure was upstream, in segmentation, plus the closed-set nature of NN.
</details>
