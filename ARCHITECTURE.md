# Architecture

One C++ recognition pipeline, three ways to run it. The same source files run as native
programs on a laptop and, compiled to WebAssembly, inside a browser tab. That last route is
how the public demo works with no server at all. Decision IDs (D-xx) refer to
[docs/mentorship/decisions.md](docs/mentorship/decisions.md), where each choice is recorded with
the alternatives that were rejected.

## 1. Three runtimes, one pipeline

```mermaid
flowchart LR
    subgraph shared["Shared C++ (src/core + src/app)"]
        core["objrec_core<br/>threshold · morphology · regions<br/>features · classifier · evaluation"]
        app["objrec_app<br/>pipeline · Session · ROI<br/>auto-learn · feature DB"]
        core --> app
    end

    app --> live["live.exe<br/>OpenCV windows"]
    app --> tools["train / eval / inspect / invariance<br/>batch tools"]
    app --> server["server.exe<br/>HTTP + WebSocket on 127.0.0.1"]
    app --> wasm["objrec.wasm<br/>Emscripten build"]

    server <--> gui1["Next.js GUI<br/>(local, npm run dev)"]
    wasm --> worker["Web Worker in the browser"]
    worker <--> gui2["Next.js GUI<br/>(static, Netlify)"]
```

| Runtime | Frames come from | ResNet18 runs in | Used for |
|---|---|---|---|
| `live.exe` | camera, IP Webcam URL, video or image folder | OpenCV dnn | development, capturing training images |
| `server.exe` + GUI | the GUI's camera over a WebSocket, or a fixed source | OpenCV dnn | local GUI with saving to disk |
| browser build | the browser camera, or bundled sample images | onnxruntime-web (WebGPU, else WebAssembly) | the public demo, phones |

The browser build exists because free hosting for a C++ server is gone: Hugging Face Docker
Spaces need a paid plan and the free alternatives run on a tenth of a CPU (D-26, D-27).

## 2. What happens to one frame

```mermaid
flowchart TD
    F["Frame (BGR, scaled to 640 px wide)"] --> S["Saturation darkening<br/>V × (1 − S): coloured objects turn dark (D-22)"]
    S --> T1["T1 · ISODATA threshold<br/>k-means K=2 on a 1/16 sample → t; pixel ≤ t → 255"]
    T1 --> T2["T2 · Clean-up, 8-connected grassfire<br/>closing (grow 2, shrink 2) then opening (shrink 1, grow 1)"]
    T2 --> T3["T3 · Connected components<br/>drop < 0.2% of frame, keep 5 largest;<br/>primary = most central region not touching the border"]
    T3 --> T4["T4 · Moments → centroid, θ, extents → oriented box<br/>features: percentFilled, aspectRatio, hu1, hu2, hu3"]
    T4 --> C{Classifier}
    C -->|hand features| NN1["Scaled Euclidean nearest neighbour<br/>σ floored at 1% of the mean · cutoff 3.0"]
    C -->|ResNet18| ROI["T9 · Rotate original frame by θ,<br/>fix 180° with third moment, crop the box"]
    ROI --> EMB["224×224, ImageNet-normalised → ResNet18<br/>512-d global-average-pool output"]
    EMB --> NN2["Cosine (or SSD) nearest neighbour · cutoff 0.35"]
    NN1 --> OUT["Label or 'unknown' → overlay, status, auto-learn, evaluation"]
    NN2 --> OUT
```

Every stage is a pure `cv::Mat` in, `cv::Mat` out function with no windows or cameras. That
rule, set on day one, is what later let the whole pipeline compile to WebAssembly unchanged.

## 3. Code layers

```mermaid
flowchart BT
    subgraph core["src/core: stages, no I/O"]
        th[threshold.cpp] ; mo[morphology.cpp] ; re[regions.cpp] ; fe[features.cpp] ; cl[classifier.cpp] ; ev[evaluation.cpp] ; pr[preprocess.cpp]
    end
    subgraph app["src/app: glue"]
        pi[pipeline.cpp<br/>runs T1–T4]
        se[session.cpp<br/>DBs, modes, classify,<br/>auto-learn, overlays]
        ro[roi.cpp<br/>T9 alignment + tensor]
        em[embedding.cpp<br/>OpenCV dnn]
        al[auto_learn.cpp]
    end
    subgraph apps["programs"]
        sv[server.cpp] ; lv[live.cpp] ; tr[train / eval / inspect]
        wb[wasm/objrec_wasm.cpp<br/>C API for the worker]
    end
    core --> app --> apps
```

`Session` holds everything between "a frame arrived" and "here is the status": both
databases, the label, the mode, the chosen classifier and metric, auto-learn and live
evaluation. `server.exe` and the browser build both drive the same `Session`, so the GUI
behaves the same against either (D-27). Embedding is two-phase, `process()` → embed the ROI →
`finish()`, because the browser's ResNet18 is asynchronous.

## 4. Browser build (the public demo)

```mermaid
sequenceDiagram
    autonumber
    participant Cam as Camera / sample images
    participant UI as GUI (main thread)
    participant W as Worker
    participant C as C++ (WebAssembly)
    participant R as ResNet18 (onnxruntime-web)

    UI->>W: start: load objrec.wasm, write DB CSVs to its virtual FS, import stored samples
    W-->>UI: ready (hand features usable now)
    W->>R: download zoo model in background, check SHA-256, add embedding output
    loop every frame, one in flight
        Cam->>UI: ImageBitmap (640 px wide, aspect kept)
        UI->>W: frame (transferred, no copy)
        W->>C: orec_process(): T1–T4, ROI if needed
        opt ResNet18 selected or auto-learn collecting
            W->>R: run on 1×3×224×224 tensor
            R-->>W: 512-d embedding
        end
        W->>C: orec_finish(embedding): classify, auto-learn, overlay
        W-->>UI: status JSON (+ threshold/regions image if that view is open)
        UI->>UI: live video with SVG overlay from status geometry
    end
```

- **Live view (D-29).** The annotated view shows the camera itself at full frame rate and draws the box, axis and label as SVG from the latest status. Before this, the picture refreshed only as fast as recognition ran.
- **Model (D-27).** ResNet18 comes straight from the ONNX model zoo's file host and is checked against the pinned SHA-256. onnxruntime returns only declared outputs, so the worker splices a 46-byte protobuf field into the model to declare the embedding layer as an output; the weights are untouched.
- **Speed.** Hand features ~10 fps. ResNet18 ~15 fps on WebGPU, ~3.6 fps on single-threaded WebAssembly.
- **Correctness.** The WebAssembly build gives the same thresholds, features and predictions as `server.exe` (relative difference 0), and the browser embedding matches OpenCV dnn's (cosine distance 8e-8).

### Samples that survive a refresh (D-28)

```mermaid
flowchart LR
    T[Train or auto-learn sample] --> S[Session adds it to both DBs]
    S --> P[Worker stores label, features, embedding,<br/>thumbnail of the crop → IndexedDB]
    L[Page load] --> I[Worker reads IndexedDB → orec_import each sample]
    D[Delete / Clear in gallery] --> R[Reset DB to the built-in set,<br/>re-import what is left]
```

Up to 50 samples per browser, on that device only.

## 5. Local server mode

```mermaid
sequenceDiagram
    autonumber
    participant UI as GUI (browser)
    participant WS as /ws/frames handler
    participant MB as FrameMailbox
    participant PL as Pipeline thread
    participant H as HTTP handlers

    UI->>WS: JPEG frame
    WS->>MB: put(frame), replacing any unread one
    PL->>MB: take(frame)
    MB-->>WS: consumed
    WS-->>UI: {"ok":true} (the client sends the next frame only now)
    PL->>PL: Session.process / embed / finish, encode views
    UI->>H: GET /stream?view=… (MJPEG), GET /api/status every 250 ms
    UI->>H: POST /api/train etc.
    H->>PL: command queued, handler waits for the reply
```

- **Flow control (H-50).** One frame in flight: a slow pipeline lowers the frame rate instead of growing a queue (which would show you the past and eventually run out of memory).
- **Threading.** Only the pipeline thread touches the `Session`. HTTP handlers queue commands and wait for the result, so no locks guard the databases.
- **Security.** Binds 127.0.0.1, rejects other websites by `Origin` (WebSockets are not covered by CORS), and Next binds to loopback too, so the LAN can't reach the API through its proxy.

## 6. Deployment

```mermaid
flowchart LR
    GH[GitHub main] --> N[Netlify build<br/>npm run build:wasm]
    N --> O["web/out (static)<br/>HTML/JS · objrec.wasm · ORT runtime · DB CSVs · samples"]
    O --> B[Browser]
    Z[ONNX model zoo] -- "ResNet18, fetched by the worker" --> B
```

[netlify.toml](netlify.toml) sets JavaScript content types for the `.mjs` modules (module
workers reject anything else) and caching. The WebAssembly module is committed because
Netlify's build image has no Emscripten; [wasm/build.sh](wasm/build.sh) reproduces it.

## 7. Key decisions

| ID | Decision | Rejected |
|---|---|---|
| D-04 | Pure `cv::Mat` stages, one executable per job | stages with I/O inside |
| D-09 | OpenCV connected components for T3; time went to extensions | from-scratch two-pass labelling |
| D-14 | Separate test captures; leave-one-out and one-shot as extra protocols | testing on training images |
| D-16 | ResNet18 embeddings for T9 | YOLO: a closed-set detector that needs retraining with boxes |
| D-17 | Auto-learn takes one sample per pose | bursts of near-identical frames |
| D-22 | Saturation darkening before ISODATA | plain grayscale (coloured objects vanished) |
| D-24 | Browser camera over WebSocket, one frame in flight | POST per frame |
| D-27 | Pipeline in the browser (WebAssembly + onnxruntime-web) | paid hosting, a TypeScript rewrite |
| D-29 | Live video with a drawn overlay; ResNet18 on WebGPU | embedding every Nth frame |
