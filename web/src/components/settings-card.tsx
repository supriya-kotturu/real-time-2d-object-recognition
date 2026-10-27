"use client";

import { useState } from "react";
import { toast } from "sonner";

import { Card, CardContent, CardDescription, CardHeader, CardTitle } from "@/components/ui/card";
import { Slider } from "@/components/ui/slider";
import { useWasmModel } from "@/hooks/use-wasm-model";
import {
  api,
  CLASSIFIERS,
  errorMessage,
  metricInfo,
  type Classifier,
  type Metric,
  type Status,
} from "@/lib/api";
import { USE_WASM } from "@/lib/wasm-backend";

const first = (v: number | readonly number[]) => (Array.isArray(v) ? v[0] : (v as number));

export function SettingsCard({ status }: { status: Status | null }) {
  // Non-null only while dragging, so polling can't yank the thumb out from under the user.
  const [draft, setDraft] = useState<number | null>(null);
  const classifier = status?.classifier ?? "features";
  const metric = status?.metric ?? "scaled-euclidean";
  const range = metricInfo(metric);
  const value = draft ?? status?.unknownThreshold ?? range.min;
  const metrics = CLASSIFIERS.find((c) => c.value === classifier)!.metrics;
  const model = useWasmModel();

  const commit = async (next: number) => {
    try {
      await api.setUnknownThreshold(next);
    } catch (err) {
      toast.error(errorMessage(err));
    } finally {
      setDraft(null);
    }
  };

  // Omitting the metric lets the server pick the classifier's default (cosine for ResNet18).
  const choose = (next: Classifier, nextMetric?: Metric) =>
    api.setClassifier(next, nextMetric).catch((err) => toast.error(errorMessage(err)));

  return (
    <Card>
      <CardHeader>
        <CardTitle>Classifier</CardTitle>
        <CardDescription>Nearest neighbour; matches beyond the cutoff are unknown</CardDescription>
      </CardHeader>
      <CardContent className="control-stack">
        <label className="setting-row">
          <span>Method</span>
          <select
            className="setting-select"
            value={classifier}
            disabled={!status}
            onChange={(e) => choose(e.target.value as Classifier)}
          >
            {CLASSIFIERS.map((c) => (
              <option
                key={c.value}
                value={c.value}
                disabled={c.value === "resnet18" && !status?.resnetAvailable}
              >
                {c.label}
              </option>
            ))}
          </select>
        </label>
        <label className="setting-row">
          <span>Distance</span>
          <select
            className="setting-select"
            value={metric}
            disabled={!status || metrics.length < 2}
            onChange={(e) => choose(classifier, e.target.value as Metric)}
          >
            {metrics.map((m) => (
              <option key={m.value} value={m.value}>
                {m.label}
              </option>
            ))}
          </select>
        </label>
        <div className="setting-row">
          <span>Unknown cutoff</span>
          <span className="feature-value">{formatDistance(value, metric)}</span>
        </div>
        <Slider
          // Remount on metric change: the old thumb position means nothing on a new scale.
          key={metric}
          min={range.min}
          max={range.max}
          step={range.step}
          value={[value]}
          disabled={!status}
          onValueChange={(v) => setDraft(first(v))}
          onValueCommitted={(v) => commit(first(v))}
        />
        {USE_WASM && model.state === "loading" && (
          <p className="setting-note">
            Downloading ResNet18 from the ONNX model zoo (47 MB, once; your browser caches it):{" "}
            {Math.round(model.progress * 100)}%. Hand features work meanwhile.
          </p>
        )}
        {USE_WASM && model.state === "error" && (
          <p className="setting-note">ResNet18 unavailable: {model.message}</p>
        )}
        {classifier === "resnet18" ? (
          <p className="setting-note">
            ResNet18 compares whole-image embeddings: 47/47 on our test split.{" "}
            {!USE_WASM
              ? "Slower than hand features (~40 ms per frame on a laptop CPU)"
              : model.state === "ready" && model.backend === "webgpu"
                ? "Running on your GPU via WebGPU"
                : "Slower here (~250 ms per frame: this browser offers no WebGPU, so it runs on the CPU)"}
            , and sensitive to lighting changes. Switch to Hand features to compare.
          </p>
        ) : (
          <p className="setting-note">
            Hand features use shape only (fill, aspect ratio, Hu moments): fast, but 26/47 on our
            test split, since round blob-like objects look alike.
          </p>
        )}
      </CardContent>
    </Card>
  );
}

export const formatDistance = (d: number, metric: Metric) =>
  d.toFixed(metric === "cosine" ? 3 : metric === "ssd" ? 0 : 2);
