// Typed client for server.exe's JSON API (D-13). Shapes mirror src/apps/server.cpp.

// Rewrite proxies may buffer multipart responses, so <img> loads the stream straight from
// the C++ server instead of through Next.
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

export interface Status {
  threshold: number;
  label: string;
  source: string;
  still: boolean;
  dbSize: number;
  unknownThreshold: number;
  fps: number;
  object: DetectedObject | null;
}

export interface DbSummary {
  total: number;
  labels: { label: string; count: number }[];
}

// Mirrors isValidLabel() in C++; the server re-checks, this just gives instant feedback.
export const LABEL_PATTERN = /^[A-Za-z0-9_-]{1,64}$/;

export class ApiError extends Error {}

async function request<T>(path: string, init?: RequestInit): Promise<T> {
  const res = await fetch(path, {
    cache: "no-store",
    ...init,
    headers: { "Content-Type": "application/json", ...init?.headers },
  });
  const body = await res.json().catch(() => ({}));
  if (!res.ok) throw new ApiError(body.error ?? `HTTP ${res.status}`);
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
  capture: () => request<{ saved: string }>("/api/capture", { method: "POST" }),
  next: () => request<object>("/api/next", { method: "POST" }),
  setUnknownThreshold: (unknownThreshold: number) =>
    request<{ unknownThreshold: number }>("/api/settings", {
      method: "PUT",
      body: JSON.stringify({ unknownThreshold }),
    }),
};

export const streamUrl = (view: View) => `${STREAM_ORIGIN}/stream?view=${view}`;

export const errorMessage = (err: unknown) =>
  err instanceof Error ? err.message : "Unexpected error";
