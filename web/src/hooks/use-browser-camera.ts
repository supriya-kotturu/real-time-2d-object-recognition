"use client";

import { useCallback, useEffect, useRef, useState } from "react";

import { framesSocketUrl } from "@/lib/api";
import { connectServerSink, connectWasmSink, type FrameSink } from "@/lib/frame-sink";
import { USE_WASM } from "@/lib/wasm-backend";

export type CameraPhase = "idle" | "starting" | "live" | "error";
export type FrameSourceKind = "camera" | "samples";

// The pipeline's pixel-based parameters were tuned on 640x480 (D-07, D-21). Scaling keeps the
// camera's aspect ratio: squashing 16:9 into 4:3 would change the aspectRatio feature.
const FRAME_WIDTH = 640;

// D-26: one held-out test capture per label (data/test), so visitors without the physical
// objects can still watch recognition work. Each is shown long enough for the overlay to settle.
export const SAMPLE_LABELS = [
  "ball", "batteries", "cloud", "keychain", "kitchen-towel",
  "mouse", "notebook", "spects", "sticky-notes", "toy-car",
];
const SAMPLE_SECONDS = 3;
const SAMPLE_FPS = 10; // the frames are identical, so more would only burn server CPU

const sleep = (ms: number) => new Promise((r) => setTimeout(r, ms));

function describe(err: unknown): string {
  if (err instanceof DOMException) {
    if (err.name === "NotAllowedError") return "Camera permission denied";
    if (err.name === "NotFoundError" || err.name === "OverconstrainedError")
      return "No camera found";
    if (err.name === "NotReadableError") return "Camera is in use by another app";
  }
  return err instanceof Error ? err.message : "Camera failed";
}

function grabFrame(video: HTMLVideoElement): Promise<ImageBitmap> {
  const width = Math.min(FRAME_WIDTH, video.videoWidth);
  const height = Math.round((width * video.videoHeight) / video.videoWidth);
  return createImageBitmap(video, { resizeWidth: width, resizeHeight: height, resizeQuality: "high" });
}

export function useBrowserCamera() {
  const [phase, setPhase] = useState<CameraPhase>("idle");
  const [error, setError] = useState<string | null>(null);
  const [devices, setDevices] = useState<MediaDeviceInfo[]>([]);
  const [deviceId, setDeviceId] = useState<string>("");
  const [sentFps, setSentFps] = useState(0);
  const [kind, setKind] = useState<FrameSourceKind>("camera");
  const [sampleLabel, setSampleLabel] = useState<string | null>(null);
  // The live stream, so the GUI can show it directly instead of the processed frames (D-29).
  const [stream, setStream] = useState<MediaStream | null>(null);

  // Each start bumps the generation; a loop whose generation is stale stops itself, so a
  // quick stop → start never leaves two loops sending (H-04).
  const generation = useRef(0);
  const session = useRef<{ stream: MediaStream | null; sink: FrameSink | null } | null>(null);

  const teardown = useCallback(() => {
    generation.current++;
    session.current?.sink?.close();
    session.current?.stream?.getTracks().forEach((t) => t.stop());
    session.current = null;
    setStream(null);
  }, []);

  const stop = useCallback(() => {
    teardown();
    setPhase("idle");
    setSentFps(0);
    setSampleLabel(null);
  }, [teardown]);

  // Shared by both sources: connect, then send whatever `next` produces, one frame in flight.
  const pump = useCallback(
    async (gen: number, next: () => Promise<ImageBitmap>, minIntervalMs = 0) => {
      const sink = USE_WASM ? await connectWasmSink() : await connectServerSink(framesSocketUrl());
      if (gen !== generation.current) {
        sink.close();
        return;
      }
      session.current = { stream: session.current?.stream ?? null, sink };
      setPhase("live");

      let frames = 0;
      let windowStart = performance.now();
      while (gen === generation.current) {
        const sentAt = performance.now();
        await sink.send(await next());
        frames++;
        const now = performance.now();
        // State updates twice a second, not per frame, so the dashboard doesn't re-render at 30 Hz.
        if (now - windowStart >= 500) {
          if (gen === generation.current) setSentFps((frames * 1000) / (now - windowStart));
          frames = 0;
          windowStart = now;
        }
        if (minIntervalMs) await sleep(minIntervalMs - (now - sentAt));
      }
    },
    [],
  );

  const begin = useCallback(
    (source: FrameSourceKind) => {
      teardown();
      const gen = generation.current;
      setKind(source);
      setPhase("starting");
      setError(null);
      setSampleLabel(null);
      const fail = (err: unknown) => {
        if (gen !== generation.current) return;
        teardown();
        setError(describe(err));
        setPhase("error");
        setSentFps(0);
      };
      return { gen, fail };
    },
    [teardown],
  );

  const start = useCallback(
    async (requested?: string) => {
      const { gen, fail } = begin("camera");
      if (!navigator.mediaDevices?.getUserMedia) {
        fail(new Error("Camera needs a secure context: open the GUI via localhost or HTTPS"));
        return;
      }
      try {
        const size = { width: { ideal: 640 }, height: { ideal: 480 } };
        // Phones default to the selfie camera; pointing down at paper needs the back one.
        // "ideal" so laptops with only a front camera still get it.
        const stream = await navigator.mediaDevices.getUserMedia({
          audio: false,
          video: requested
            ? { ...size, deviceId: { exact: requested } }
            : { ...size, facingMode: { ideal: "environment" } },
        });
        if (gen !== generation.current) {
          stream.getTracks().forEach((t) => t.stop());
          return;
        }
        session.current = { stream, sink: null };
        setStream(stream);
        const track = stream.getVideoTracks()[0];
        track.onended = () => fail(new Error("Camera disconnected"));

        // Device labels are only exposed after permission is granted, so list them now.
        const all = await navigator.mediaDevices.enumerateDevices();
        setDevices(all.filter((d) => d.kind === "videoinput"));
        setDeviceId(track.getSettings().deviceId ?? requested ?? "");

        const video = document.createElement("video");
        video.muted = true;
        video.playsInline = true;
        video.srcObject = stream;
        await video.play();

        await pump(gen, () => grabFrame(video));
      } catch (err) {
        fail(err);
      }
    },
    [begin, pump],
  );

  const startSamples = useCallback(async () => {
    const { gen, fail } = begin("samples");
    try {
      const images = await Promise.all(
        SAMPLE_LABELS.map(async (label) => {
          const res = await fetch(`/samples/${label}.jpg`);
          if (!res.ok) throw new Error(`missing sample image: ${label}`);
          return createImageBitmap(await res.blob());
        }),
      );
      const startedAt = performance.now();
      let shown = -1;
      await pump(
        gen,
        async () => {
          const i =
            Math.floor((performance.now() - startedAt) / (SAMPLE_SECONDS * 1000)) % images.length;
          if (i !== shown && gen === generation.current) {
            shown = i;
            setSampleLabel(SAMPLE_LABELS[i]);
          }
          return createImageBitmap(images[i]); // a copy: the sink takes ownership
        },
        1000 / SAMPLE_FPS,
      );
    } catch (err) {
      fail(err);
    }
  }, [begin, pump]);

  useEffect(() => teardown, [teardown]);

  return {
    phase,
    error,
    devices,
    deviceId,
    sentFps,
    kind,
    sampleLabel,
    stream,
    start,
    startSamples,
    stop,
  };
}
