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
import { SampleStore, STORE_CAP, type StoredSample } from "@/worker/sample-store";

const TENSOR_SIZE = 3 * 224 * 224;
const EMBEDDING_SIZE = 512;
const VIEW_INDEX: Record<ViewName, number> = { annotated: 0, threshold: 1, regions: 2 };

// The Emscripten module's surface we use (objrec_wasm.cpp).
interface Objrec {
  HEAPU8: Uint8Array;
  HEAPF32: Float32Array;
  HEAPF64: Float64Array;
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
  _orec_added_count(): number;
  _orec_added_label(): number;
  _orec_added_features_len(): number;
  _orec_added_features(): number;
  _orec_added_embedding(): number;
  _orec_added_thumb(maxSide: number): number;
  _orec_thumb_width(): number;
  _orec_thumb_height(): number;
  _orec_import(label: number, features: number, n: number, embedding: number): void;
}

const post = (msg: FromWorker, transfer: Transferable[] = []) => self.postMessage(msg, transfer);

let M: Objrec;
let session: ort.InferenceSession | null = null;
let embeddingPtr = 0; // reused 512-float buffer handed to C++
let canvas: OffscreenCanvas | null = null;
let fps = 0;
let lastFrameAt = 0;
let store: SampleStore | null = null;
let storedCount = 0;
let seenAdded = 0; // Session's added-sample counter at the last check
const THUMB_SIDE = 96;

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
  store = await SampleStore.open();
  await importStored();
}

// --- D-28: the visitor's samples survive a refresh (IndexedDB, on this device only). ---

async function importStored() {
  const samples = store ? await store.all() : [];
  storedCount = samples.length;
  for (const s of samples) {
    const label = M.stringToNewUTF8(s.label);
    const features = M._malloc(s.features.length * 8);
    M.HEAPF64.set(s.features, features / 8);
    let embedding = 0;
    if (s.embedding) {
      embedding = M._malloc(EMBEDDING_SIZE * 4);
      M.HEAPF32.set(s.embedding, embedding / 4);
    }
    M._orec_import(label, features, s.features.length, embedding);
    [label, features, embedding].forEach((p) => p && M._free(p));
  }
}

// Writes the sample Session just added (Train, or auto-learn during a frame) to IndexedDB.
// Past the cap it stays in memory for this tab only; the commands below refuse before that.
async function persistAdded() {
  const added = M._orec_added_count();
  if (added === seenAdded) return;
  seenAdded = added;
  if (!store || storedCount >= STORE_CAP) return;

  const n = M._orec_added_features_len();
  const fPtr = M._orec_added_features();
  const ePtr = M._orec_added_embedding();
  const sample: StoredSample = {
    label: M.UTF8ToString(M._orec_added_label()),
    features: Array.from(M.HEAPF64.subarray(fPtr / 8, fPtr / 8 + n)),
    embedding: ePtr ? Float32Array.from(M.HEAPF64.subarray(ePtr / 8, ePtr / 8 + EMBEDDING_SIZE)) : null,
    thumb: null,
    createdAt: Date.now(),
  };
  const tPtr = M._orec_added_thumb(THUMB_SIDE);
  if (tPtr) {
    const w = M._orec_thumb_width(), h = M._orec_thumb_height();
    const rgba = new Uint8ClampedArray(M.HEAPU8.buffer, tPtr, w * h * 4).slice();
    const thumb = new OffscreenCanvas(w, h);
    thumb.getContext("2d")!.putImageData(new ImageData(rgba, w, h), 0, 0);
    sample.thumb = await thumb.convertToBlob({ type: "image/jpeg", quality: 0.85 });
  }
  try {
    await store.add(sample);
    storedCount++;
  } catch {
    // Quota or private mode: the sample still works for this tab.
  }
}

// Back to the built-in training set plus whatever is still stored.
async function rebuild() {
  command("resetDb");
  await importStored();
  seenAdded = M._orec_added_count();
}

function galleryReply() {
  return (async () => {
    const samples = store ? await store.all() : [];
    const reader = new FileReaderSync();
    const items = samples.map((s) => ({
      id: s.id,
      label: s.label,
      createdAt: s.createdAt,
      thumb: s.thumb ? reader.readAsDataURL(s.thumb) : null,
    }));
    return { status: 200, body: JSON.stringify({ cap: STORE_CAP, available: store !== null, items }) };
  })();
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
  await persistAdded(); // auto-learn adds samples inside finish

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

const full = () => ({
  status: 409,
  body: JSON.stringify({
    error: `This browser keeps up to ${STORE_CAP} of your samples (${storedCount} stored). Delete some under "Your samples" first.`,
  }),
});

async function runCommand(msg: Extract<ToWorker, { type: "command" }>) {
  switch (msg.name) {
    case "gallery":
      return galleryReply();
    case "galleryDelete":
      await store?.remove(Number(msg.a));
      await rebuild();
      return galleryReply();
    case "galleryClear":
    case "resetDb": // "Reset DB" means back to the original training set, so storage goes too
      await store?.clear();
      await rebuild();
      return msg.name === "resetDb" ? command("db") : galleryReply();
    case "train":
      if (store && storedCount >= STORE_CAP) return full();
      break;
    case "autoLearnLabel": // one run collects 5 poses
      if (store && storedCount > STORE_CAP - 5) return full();
      break;
  }

  let embedding = 0;
  // train adds to both DBs, so it needs this frame's embedding even on hand features.
  if (msg.name === "train" && session) {
    const tensor = M._orec_current_roi_tensor();
    if (tensor) embedding = await embed(tensor);
  }
  const reply = command(msg.name, msg.a, msg.b, msg.n, embedding);
  if (reply.status < 400) {
    if (msg.name === "train") await persistAdded();
    if (msg.name === "deleteLabel" && store) {
      await store.removeLabel(msg.a ?? "");
      storedCount = await store.count();
    }
  }
  return reply;
}

async function onCommand(msg: Extract<ToWorker, { type: "command" }>) {
  post({ type: "reply", id: msg.id, ...(await runCommand(msg)) });
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
