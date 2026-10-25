/// <reference lib="webworker" />
// D-27: the whole recognition pipeline in the browser, off the main thread (H-03). Tasks 1-4,
// the DBs and classification run in our C++ compiled to WebAssembly (public/wasm, the same
// Session code as server.exe); ResNet18 runs in onnxruntime-web.

import * as ort from "onnxruntime-web";

import {
  declareEmbeddingOutput,
  EMBEDDING_OUTPUT,
  MODEL_SHA256,
  MODEL_URL,
  sha256Hex,
} from "@/lib/onnx-patch";
import type { FromWorker, ModelState, ToWorker, ViewName } from "@/worker/protocol";

const TENSOR_SIZE = 3 * 224 * 224;
const EMBEDDING_SIZE = 512;
const VIEW_INDEX: Record<ViewName, number> = { annotated: 0, threshold: 1, regions: 2 };

// The Emscripten module's surface we use (objrec_wasm.cpp).
interface Objrec {
  HEAPU8: Uint8Array;
  HEAPF32: Float32Array;
  FS: { mkdirTree(p: string): void; writeFile(p: string, d: Uint8Array): void };
  UTF8ToString(p: number): string;
  stringToNewUTF8(s: string): number;
  _malloc(n: number): number;
  _free(p: number): void;
  _orec_init(embeddingAvailable: number): number;
  _orec_set_embedding_available(a: number): void;
  _orec_frame_buffer(w: number, h: number): number;
  _orec_process(): number;
  _orec_roi_tensor(): number;
  _orec_current_roi_tensor(): number;
  _orec_finish(embedding: number): void;
  _orec_view(which: number): number;
  _orec_status(fps: number): number;
  _orec_command(name: number, a: number, b: number, n: number, embedding: number): number;
  _orec_last_status(): number;
}

const post = (msg: FromWorker, transfer: Transferable[] = []) => self.postMessage(msg, transfer);

let M: Objrec;
let session: ort.InferenceSession | null = null;
let embeddingPtr = 0; // reused 512-float buffer handed to C++
let canvas: OffscreenCanvas | null = null;
let fps = 0;
let lastFrameAt = 0;

const status = () => M.UTF8ToString(M._orec_status(fps));

async function loadPipeline() {
  // Runtime URL, not a bundled import: the module is a build artifact in public/wasm.
  const url = new URL("/wasm/objrec.mjs", self.location.origin).href;
  const { default: createObjrec } = await import(/* turbopackIgnore: true */ url);
  M = (await createObjrec()) as Objrec;
  M.FS.mkdirTree("/data/db");
  for (const file of ["features.csv", "embeddings_resnet18.csv"]) {
    const res = await fetch(`/db/${file}`);
    if (!res.ok) throw new Error(`missing /db/${file}`);
    M.FS.writeFile(`/data/db/${file}`, new Uint8Array(await res.arrayBuffer()));
  }
  // Starts on hand features; switches to ResNet18 once the model has loaded (below), so the
  // page is usable before the 47 MB download finishes.
  M._orec_init(0);
  embeddingPtr = M._malloc(EMBEDDING_SIZE * 4);
}

async function download(onProgress: (p: number) => void): Promise<Uint8Array> {
  const res = await fetch(MODEL_URL);
  if (!res.ok || !res.body) throw new Error(`model download failed: HTTP ${res.status}`);
  const total = Number(res.headers.get("content-length")) || 46_806_737;
  const reader = res.body.getReader();
  const chunks: Uint8Array[] = [];
  let received = 0;
  for (;;) {
    const { done, value } = await reader.read();
    if (done) break;
    chunks.push(value);
    received += value.length;
    onProgress(Math.min(1, received / total));
  }
  const bytes = new Uint8Array(received);
  let o = 0;
  for (const c of chunks) {
    bytes.set(c, o);
    o += c.length;
  }
  return bytes;
}

async function loadModel() {
  const report = (model: ModelState) => post({ type: "model", model, status: status() });
  try {
    let lastReported = -1;
    const bytes = await download((p) => {
      const pct = Math.floor(p * 100);
      if (pct !== lastReported) {
        lastReported = pct;
        report({ state: "loading", progress: p });
      }
    });
    if ((await sha256Hex(bytes)) !== MODEL_SHA256)
      throw new Error("ResNet18 download does not match the pinned SHA-256");
    // Single-threaded wasm: threads would need cross-origin isolation (COOP/COEP, H-09).
    ort.env.wasm.numThreads = 1;
    ort.env.wasm.wasmPaths = "/ort/";
    const loaded = await ort.InferenceSession.create(declareEmbeddingOutput(bytes), {
      executionProviders: ["wasm"],
    });
    // The switch goes through the queue so it never lands between a frame's process and
    // finish; the download above runs alongside frames.
    queue = queue.then(() => {
      session = loaded;
      M._orec_set_embedding_available(1);
      command("setClassifier", "resnet18", "cosine");
      report({ state: "ready" });
    });
  } catch (err) {
    report({ state: "error", message: err instanceof Error ? err.message : String(err) });
  }
}

// Runs ResNet18 on the tensor C++ prepared at `tensorPtr`; returns the C++-side embedding ptr.
async function embed(tensorPtr: number): Promise<number> {
  const input = M.HEAPF32.slice(tensorPtr / 4, tensorPtr / 4 + TENSOR_SIZE);
  const out = await session!.run({ data: new ort.Tensor("float32", input, [1, 3, 224, 224]) }, [
    EMBEDDING_OUTPUT,
  ]);
  // Memory may have grown during the await, so index the heap only now.
  M.HEAPF32.set(out[EMBEDDING_OUTPUT].data as Float32Array, embeddingPtr / 4);
  return embeddingPtr;
}

function command(name: string, a = "", b = "", n = 0, embedding = 0) {
  const ptrs = [M.stringToNewUTF8(name), M.stringToNewUTF8(a), M.stringToNewUTF8(b)];
  const body = M.UTF8ToString(M._orec_command(ptrs[0], ptrs[1], ptrs[2], n, embedding));
  ptrs.forEach((p) => M._free(p));
  return { status: M._orec_last_status(), body };
}

async function onFrame(id: number, frame: ImageBitmap, viewName: ViewName) {
  const { width, height } = frame;
  if (!canvas || canvas.width !== width || canvas.height !== height)
    canvas = new OffscreenCanvas(width, height);
  const ctx = canvas.getContext("2d", { willReadFrequently: true })!;
  ctx.drawImage(frame, 0, 0);
  frame.close();
  const pixels = ctx.getImageData(0, 0, width, height).data;
  M.HEAPU8.set(pixels, M._orec_frame_buffer(width, height));

  const needsEmbedding = M._orec_process() === 1 && session !== null;
  M._orec_finish(needsEmbedding ? await embed(M._orec_roi_tensor()) : 0);

  const now = performance.now();
  if (lastFrameAt) {
    const dt = (now - lastFrameAt) / 1000;
    fps = fps === 0 ? 1 / dt : 0.9 * fps + 0.1 / dt;
  }
  lastFrameAt = now;

  let view: ImageBitmap | null = null;
  const viewPtr = M._orec_view(VIEW_INDEX[viewName]);
  if (viewPtr) {
    const rgba = new Uint8ClampedArray(M.HEAPU8.buffer, viewPtr, width * height * 4).slice();
    view = await createImageBitmap(new ImageData(rgba, width, height));
  }
  post({ type: "frameDone", id, status: status(), view }, view ? [view] : []);
}

async function onCommand(msg: Extract<ToWorker, { type: "command" }>) {
  let embedding = 0;
  // train adds to both DBs, so it needs this frame's embedding even on hand features.
  if (msg.name === "train" && session) {
    const tensor = M._orec_current_roi_tensor();
    if (tensor) embedding = await embed(tensor);
  }
  const reply = command(msg.name, msg.a, msg.b, msg.n, embedding);
  post({ type: "reply", id: msg.id, ...reply });
}

// Messages are handled strictly in order: the Session isn't re-entrant, and a frame awaiting
// ResNet18 must not interleave with a command touching the same state.
let queue = Promise.resolve();
self.onmessage = (event: MessageEvent<ToWorker>) => {
  const msg = event.data;
  queue = queue
    .then(() => (msg.type === "frame" ? onFrame(msg.id, msg.frame, msg.view) : onCommand(msg)))
    .catch((err) => {
      if (msg.type === "command")
        post({ type: "reply", id: msg.id, status: 500, body: JSON.stringify({ error: String(err) }) });
      else post({ type: "frameDone", id: msg.id, status: status(), view: null });
    });
};

loadPipeline()
  .then(() => {
    post({ type: "ready", status: status() });
    void loadModel();
  })
  .catch((err) => post({ type: "fatal", message: err instanceof Error ? err.message : String(err) }));
