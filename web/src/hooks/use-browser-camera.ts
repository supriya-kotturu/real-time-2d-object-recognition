"use client";

import { useCallback, useEffect, useRef, useState } from "react";

import { framesSocketUrl } from "@/lib/api";
import { connectServerSink, type FrameSink } from "@/lib/frame-sink";

export type CameraPhase = "idle" | "starting" | "live" | "error";

// The pipeline's pixel-based parameters were tuned on 640x480 (D-07, D-21). Scaling keeps the
// camera's aspect ratio: squashing 16:9 into 4:3 would change the aspectRatio feature.
const FRAME_WIDTH = 640;
const JPEG_QUALITY = 0.8;

function describe(err: unknown): string {
  if (err instanceof DOMException) {
    if (err.name === "NotAllowedError") return "Camera permission denied";
    if (err.name === "NotFoundError" || err.name === "OverconstrainedError")
      return "No camera found";
    if (err.name === "NotReadableError") return "Camera is in use by another app";
  }
  return err instanceof Error ? err.message : "Camera failed";
}

function grabJpeg(video: HTMLVideoElement, canvas: HTMLCanvasElement): Promise<Blob> {
  const width = Math.min(FRAME_WIDTH, video.videoWidth);
  const height = Math.round((width * video.videoHeight) / video.videoWidth);
  if (canvas.width !== width) canvas.width = width;
  if (canvas.height !== height) canvas.height = height;
  canvas.getContext("2d")!.drawImage(video, 0, 0, width, height);
  return new Promise((resolve, reject) =>
    canvas.toBlob((b) => (b ? resolve(b) : reject(new Error("JPEG encode failed"))), "image/jpeg", JPEG_QUALITY),
  );
}

export function useBrowserCamera() {
  const [phase, setPhase] = useState<CameraPhase>("idle");
  const [error, setError] = useState<string | null>(null);
  const [devices, setDevices] = useState<MediaDeviceInfo[]>([]);
  const [deviceId, setDeviceId] = useState<string>("");
  const [sentFps, setSentFps] = useState(0);

  // Each start() bumps the generation; a loop whose generation is stale stops itself, so a
  // quick stop → start never leaves two loops sending (H-04).
  const generation = useRef(0);
  const session = useRef<{ stream: MediaStream; sink: FrameSink | null } | null>(null);

  const teardown = useCallback(() => {
    generation.current++;
    session.current?.sink?.close();
    session.current?.stream.getTracks().forEach((t) => t.stop());
    session.current = null;
  }, []);

  const stop = useCallback(() => {
    teardown();
    setPhase("idle");
    setSentFps(0);
  }, [teardown]);

  const start = useCallback(
    async (requested?: string) => {
      teardown();
      const gen = generation.current;
      const fail = (err: unknown) => {
        if (gen !== generation.current) return;
        teardown();
        setError(describe(err));
        setPhase("error");
        setSentFps(0);
      };

      if (!navigator.mediaDevices?.getUserMedia) {
        fail(new Error("Camera needs a secure context: open the GUI via localhost or HTTPS"));
        return;
      }
      setPhase("starting");
      setError(null);

      try {
        const size = { width: { ideal: 640 }, height: { ideal: 480 } };
        const stream = await navigator.mediaDevices.getUserMedia({
          audio: false,
          video: requested ? { ...size, deviceId: { exact: requested } } : size,
        });
        if (gen !== generation.current) {
          stream.getTracks().forEach((t) => t.stop());
          return;
        }
        session.current = { stream, sink: null };
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

        const sink = await connectServerSink(framesSocketUrl);
        if (gen !== generation.current) {
          sink.close();
          return;
        }
        session.current.sink = sink;
        setPhase("live");

        const canvas = document.createElement("canvas");
        let frames = 0;
        let windowStart = performance.now();
        while (gen === generation.current) {
          await sink.send(await grabJpeg(video, canvas));
          frames++;
          const now = performance.now();
          // State updates twice a second, not per frame, so the dashboard doesn't re-render at 30 Hz.
          if (now - windowStart >= 500) {
            if (gen === generation.current) setSentFps((frames * 1000) / (now - windowStart));
            frames = 0;
            windowStart = now;
          }
        }
      } catch (err) {
        fail(err);
      }
    },
    [teardown],
  );

  useEffect(() => teardown, [teardown]);

  return { phase, error, devices, deviceId, sentFps, start, stop };
}
