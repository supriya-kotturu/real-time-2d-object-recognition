// Typed client for server.exe's JSON API (D-13). Shapes mirror src/apps/server.cpp.

import { USE_WASM, wasmRequest } from "@/lib/wasm-backend";

// Rewrite proxies may buffer multipart responses, so <img> loads the stream straight from
// the C++ server instead of through Next.
// The static export served by server.exe --public (D-26) builds with this set to "", making
// every URL same-origin.
export const STREAM_ORIGIN = process.env.NEXT_PUBLIC_STREAM_ORIGIN ?? "http://127.0.0.1:8080";

export type View = "annotated" | "threshold" | "regions";

export const VIEWS: { value: View; label: string }[] = [
  { value: "annotated", label: "Annotated" },
  { value: "threshold", label: "Threshold" },
  { value: "regions", label: "Regions" },
];

export interface Prediction {
  label: string;
  distance: number;
  known: boolean;
}

export interface DetectedObject {
  features: Record<string, number>;
  prediction: Prediction | null;
}

export type Mode = "detect" | "collect" | "evaluate";

export const MODES: { value: Mode; label: string }[] = [
  { value: "detect", label: "Detect" },
  { value: "collect", label: "Collect" },
  { value: "evaluate", label: "Evaluate" },
];

export interface AutoLearnStatus {
  state: "idle" | "tracking" | "prompt" | "collecting";
  progress: number; // 0..1 of the hold window while tracking
  label: string;
  collected: number;
  target: number;
  phase: "move" | "hold"; // while collecting: waiting for a new pose vs settling on one
}

export type Classifier = "features" | "resnet18";
export type Metric = "scaled-euclidean" | "cosine" | "ssd";

// D-25: which metrics each classifier offers; the first is its default. Slider ranges follow
// each metric's scale (eval's correct-match distances), since one range can't fit all three.
export const CLASSIFIERS: {
  value: Classifier;
  label: string;
  metrics: { value: Metric; label: string; min: number; max: number; step: number }[];
}[] = [
  {
    value: "features",
    label: "Hand features",
    metrics: [{ value: "scaled-euclidean", label: "Scaled Euclidean", min: 0.5, max: 10, step: 0.1 }],
  },
  {
    value: "resnet18",
    label: "ResNet18 embedding",
    metrics: [
      { value: "cosine", label: "Cosine distance", min: 0.05, max: 1, step: 0.01 },
      { value: "ssd", label: "Sum of squared differences", min: 50, max: 1000, step: 10 },
    ],
  },
];

export const metricInfo = (metric: Metric) =>
  CLASSIFIERS.flatMap((c) => c.metrics).find((m) => m.value === metric)!;

export interface Status {
  mode: Mode;
  classifier: Classifier;
  metric: Metric;
  resnetAvailable: boolean;
  embedDbSize: number;
  autoLearn: AutoLearnStatus;
  threshold: number;
  label: string;
  source: string;
  still: boolean;
  browser: boolean; // server.exe --browser: frames come from this GUI's camera (D-24)
  public: boolean; // hosted demo: DB changes are in memory only, Capture is off (D-26)
  dbSize: number;
  unknownThreshold: number;
  fps: number;
  object: DetectedObject | null;
}

export interface EvalMatrix {
  rows: string[];
  columns: string[];
  counts: number[][];
  total: number;
  accuracy: number;
}

export interface DbSummary {
  total: number;
  labels: { label: string; count: number }[];
}

// Mirrors isValidLabel() in C++; the server re-checks, this just gives instant feedback.
export const LABEL_PATTERN = /^[A-Za-z0-9_-]{1,64}$/;

export class ApiError extends Error {}

async function request<T>(path: string, init?: RequestInit): Promise<T> {
  // Browser build (D-27): the same routes, answered by the pipeline worker instead of HTTP.
  if (USE_WASM) {
    const reply = await wasmRequest(
      path,
      init?.method ?? "GET",
      init?.body ? JSON.parse(String(init.body)) : undefined,
    );
    let body: { error?: string } | null = null;
    try {
      body = JSON.parse(reply.body);
    } catch {}
    if (reply.status >= 400 || body === null) throw new ApiError(body?.error ?? `HTTP ${reply.status}`);
    return body as T;
  }
  const res = await fetch(path, {
    cache: "no-store",
    ...init,
    headers: { "Content-Type": "application/json", ...init?.headers },
  });
  // A proxy or sleeping host can answer with an HTML page, even with 200: that is not a
  // status, so it must count as offline rather than as an empty object.
  const body = await res.json().catch(() => null);
  if (!res.ok || body === null) throw new ApiError(body?.error ?? `HTTP ${res.status}`);
  return body as T;
}

export const api = {
  status: (signal?: AbortSignal) => request<Status>("/api/status", { signal }),
  db: () => request<DbSummary>("/api/db"),
  deleteLabel: (label: string) =>
    request<DbSummary>(`/api/db/${encodeURIComponent(label)}`, { method: "DELETE" }),
  setLabel: (label: string) =>
    request<{ label: string }>("/api/label", { method: "POST", body: JSON.stringify({ label }) }),
  train: () => request<DbSummary>("/api/train", { method: "POST" }),
  resetDb: () => request<DbSummary>("/api/db/reset", { method: "POST" }),
  capture: () => request<{ saved: string }>("/api/capture", { method: "POST" }),
  next: () => request<object>("/api/next", { method: "POST" }),
  setClassifier: (classifier: Classifier, metric?: Metric) =>
    request<{ classifier: Classifier; metric: Metric }>("/api/classifier", {
      method: "PUT",
      body: JSON.stringify(metric ? { classifier, metric } : { classifier }),
    }),
  setMode: (mode: Mode) =>
    request<{ mode: Mode }>("/api/mode", { method: "PUT", body: JSON.stringify({ mode }) }),
  autoLearnLabel: (label: string) =>
    request<{ collecting: string }>("/api/autolearn/label", {
      method: "POST",
      body: JSON.stringify({ label }),
    }),
  autoLearnDismiss: () => request<object>("/api/autolearn/dismiss", { method: "POST" }),
  evalMatrix: () => request<EvalMatrix>("/api/eval"),
  evalRecord: () => request<EvalMatrix>("/api/eval/record", { method: "POST" }),
  evalReset: () => request<EvalMatrix>("/api/eval", { method: "DELETE" }),
  setUnknownThreshold: (unknownThreshold: number) =>
    request<{ unknownThreshold: number }>("/api/settings", {
      method: "PUT",
      body: JSON.stringify({ unknownThreshold }),
    }),
};

export const streamUrl = (view: View) => `${STREAM_ORIGIN}/stream?view=${view}`;

// Next rewrites don't proxy WebSocket upgrades, so like the stream this goes straight to :8080.
// Same-origin builds derive it from the page, so https pages get wss.
export const framesSocketUrl = () => {
  const origin = STREAM_ORIGIN || window.location.origin;
  return `${origin.replace(/^http/, "ws")}/ws/frames`;
};

export const errorMessage = (err: unknown) =>
  err instanceof Error ? err.message : "Unexpected error";
