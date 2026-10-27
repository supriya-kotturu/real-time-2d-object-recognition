# 9. System design and the web app

See [ARCHITECTURE.md](../../ARCHITECTURE.md) for the diagrams. The points worth being able to
explain:

## Pure stages (D-04, H-10)

Every stage is `cv::Mat` in → `cv::Mat` out, with no windows, cameras or files. That let the
same code serve the live window, the batch tools, the server and, later, WebAssembly in the
browser.

## Session

One class owns all state between "a frame arrived" and "here is the status": both databases, the
label, the mode, the classifier choice, auto-learn and live evaluation. `server.exe` and the
browser build drive the same Session, so there's one implementation of the rules.

## Flow control (H-50)

*Which pipe* (MJPEG, WebSocket, WebRTC) is a different question from *how fast to push*. If the
camera produces frames faster than recognition runs, a queue grows: you see older and older
frames, then memory runs out. The fix used everywhere: **one frame in flight**. The sender waits
for an acknowledgement before sending the next frame, and the server keeps only the newest unread
frame (a single-slot "mailbox").

## Browser build, no server (D-27)

- Hosting the C++ server needed money (Hugging Face Docker Spaces: paid PRO plan; free hosts:
  0.1 CPU). **You proposed** running it all in a web worker.
- The C++ (OpenCV 5 core/imgproc/geometry + the project's code) is compiled to **WebAssembly**
  with Emscripten. Same source, same results: identical thresholds, features and predictions.
- It runs in a **Web Worker** so the page never freezes (H-03).
- ResNet18 runs in **onnxruntime-web**, on the GPU via **WebGPU** when available (~15 fps), else
  WebAssembly (~3.6 fps).
- **CORS lesson:** the model's github.com URL redirects without a CORS header, and browsers check
  every hop, so the worker fetches the final host directly.

## Live view (D-29)

The picture used to be the processed frame, so it refreshed at recognition speed. Now the GUI
shows the camera directly and draws the box/axis/label as SVG from the latest result: smooth video,
the overlay trails a moving object slightly.

## Samples that survive a refresh (D-28)

IndexedDB (the browser's database) stores each trained sample: label, features, embedding and a
thumbnail of the crop. Loaded back into the Session on start. Max 50, on that device only.

## Security in local mode

The server binds 127.0.0.1, but any website you visit could still call it (simple POSTs, and
WebSockets aren't covered by CORS). So it checks `Origin`, and the Next dev server binds to
loopback too.

## Check yourself

<details><summary>Why did the first GUI feel laggy, and what fixed it?</summary>

The displayed image *was* the processed frame, so the display rate equalled the recognition rate
(~3 fps with ResNet18 on CPU). Showing the live video and drawing the overlay separately (D-29),
and running ResNet18 on WebGPU, fixed it.
</details>

<details><summary>What would happen without "one frame in flight"?</summary>

Frames queue up faster than they're processed: latency grows without bound (you see the past),
then memory runs out.
</details>

<details><summary>Why not rewrite the pipeline in TypeScript for the browser?</summary>

Two implementations can quietly disagree, and the report's numbers come from the C++. Compiling the
same C++ guarantees the demo is the program you describe (verified: identical outputs).
</details>
