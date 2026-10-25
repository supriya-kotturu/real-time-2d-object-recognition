"use client";

// D-27: the GUI's handle on the pipeline worker. In the Netlify build (NEXT_PUBLIC_BACKEND=wasm)
// it stands in for server.exe: request() in api.ts routes /api/* here, the camera's FrameSink
// sends frames here, and the stream viewer draws the views it returns.

import type { FromWorker, ModelState, ToWorker, ViewName } from "@/worker/protocol";

export const USE_WASM = process.env.NEXT_PUBLIC_BACKEND === "wasm";

type Reply = { status: number; body: string };

class WasmBackend {
  private worker: Worker | null = null;
  private nextId = 1;
  private replies = new Map<number, (r: Reply) => void>();
  private frames = new Map<number, () => void>();
  private listeners = new Set<() => void>();
  private viewListeners = new Set<(view: ImageBitmap) => void>();

  status: string | null = null;
  fatal: string | null = null;
  model: ModelState = { state: "loading", progress: 0 };
  view: ViewName = "annotated";

  // Lazy: modules are also evaluated during prerendering, where there is no Worker.
  start() {
    if (this.worker) return;
    this.worker = new Worker(new URL("../worker/pipeline.worker.ts", import.meta.url), {
      type: "module",
    });
    this.worker.onmessage = (e: MessageEvent<FromWorker>) => this.receive(e.data);
    this.worker.onerror = (e) => {
      this.fatal = e.message || "pipeline worker failed to start";
      this.emit();
    };
  }

  private receive(msg: FromWorker) {
    switch (msg.type) {
      case "ready":
        this.status = msg.status;
        break;
      case "fatal":
        this.fatal = msg.message;
        break;
      case "model":
        this.model = msg.model;
        this.status = msg.status;
        break;
      case "frameDone":
        this.status = msg.status;
        if (msg.view) this.viewListeners.forEach((l) => l(msg.view!));
        this.frames.get(msg.id)?.();
        this.frames.delete(msg.id);
        break;
      case "reply":
        this.replies.get(msg.id)?.({ status: msg.status, body: msg.body });
        this.replies.delete(msg.id);
        return; // a reply changes state only through the next status
    }
    this.emit();
  }

  private emit() {
    this.listeners.forEach((l) => l());
  }

  subscribe(listener: () => void) {
    this.listeners.add(listener);
    return () => void this.listeners.delete(listener);
  }

  onView(listener: (view: ImageBitmap) => void) {
    this.viewListeners.add(listener);
    return () => void this.viewListeners.delete(listener);
  }

  private post(msg: ToWorker, transfer: Transferable[] = []) {
    this.start();
    this.worker!.postMessage(msg, transfer);
  }

  // Resolves once the worker has processed the frame: the same one-in-flight contract as the
  // server's ack (H-50). The frame is transferred, so the caller must not reuse it.
  sendFrame(frame: ImageBitmap): Promise<void> {
    const id = this.nextId++;
    return new Promise((resolve) => {
      this.frames.set(id, resolve);
      this.post({ type: "frame", id, frame, view: this.view }, [frame]);
    });
  }

  command(name: string, a?: string, b?: string, n?: number): Promise<Reply> {
    const id = this.nextId++;
    return new Promise((resolve) => {
      this.replies.set(id, resolve);
      this.post({ type: "command", id, name, a, b, n });
    });
  }
}

export const wasmBackend = new WasmBackend();

// server.exe's routes, as worker commands. Capture writes files, which the browser build can't.
export async function wasmRequest(path: string, method: string, body: unknown): Promise<Reply> {
  const b = (body ?? {}) as Record<string, unknown>;
  const str = (k: string) => (typeof b[k] === "string" ? (b[k] as string) : "");
  const route = `${method} ${path}`;
  wasmBackend.start();

  if (route === "GET /api/status") {
    if (wasmBackend.fatal) return { status: 500, body: JSON.stringify({ error: wasmBackend.fatal }) };
    if (!wasmBackend.status) return { status: 503, body: JSON.stringify({ error: "loading" }) };
    return { status: 200, body: wasmBackend.status };
  }
  const deleteMatch = path.match(/^\/api\/db\/([^/]+)$/);
  if (method === "DELETE" && deleteMatch)
    return wasmBackend.command("deleteLabel", decodeURIComponent(deleteMatch[1]));

  switch (route) {
    case "GET /api/db":
      return wasmBackend.command("db");
    case "POST /api/db/reset":
      return wasmBackend.command("resetDb");
    case "POST /api/label":
      return wasmBackend.command("setLabel", str("label"));
    case "POST /api/train":
      return wasmBackend.command("train");
    case "POST /api/capture":
      return { status: 403, body: JSON.stringify({ error: "capture is not available in the browser build" }) };
    case "POST /api/next":
      return { status: 200, body: "{}" };
    case "PUT /api/settings":
      return wasmBackend.command("setCutoff", "", "", Number(b.unknownThreshold) || 0);
    case "PUT /api/classifier":
      return wasmBackend.command("setClassifier", str("classifier"), str("metric"));
    case "PUT /api/mode":
      return wasmBackend.command("setMode", str("mode"));
    case "POST /api/autolearn/label":
      return wasmBackend.command("autoLearnLabel", str("label"));
    case "POST /api/autolearn/dismiss":
      return wasmBackend.command("autoLearnDismiss");
    case "GET /api/eval":
      return wasmBackend.command("evalMatrix");
    case "POST /api/eval/record":
      return wasmBackend.command("evalRecord");
    case "DELETE /api/eval":
      return wasmBackend.command("evalReset");
  }
  return { status: 404, body: JSON.stringify({ error: `no route ${route}` }) };
}
