"use client";

import { useEffect, useMemo, useRef, useSyncExternalStore } from "react";

import { wasmBackend } from "@/lib/wasm-backend";

interface Overlay {
  frame: [number, number];
  box: [number, number][];
  axis: [number, number][];
  centroid: [number, number];
  text: string;
}

// D-29: the camera shown directly (full frame rate), with the T4 overlay drawn from the latest
// result instead of baked into processed frames, which arrive only as fast as recognition runs.
// The box trails a moving object by one recognition step and catches up when it stops.
export function LiveCamera({ stream }: { stream: MediaStream }) {
  const video = useRef<HTMLVideoElement>(null);
  useEffect(() => {
    if (video.current) video.current.srcObject = stream;
  }, [stream]);

  // Every processed frame updates the status; re-render on each so the box follows promptly.
  const status = useSyncExternalStore(
    (onChange) => wasmBackend.subscribe(onChange),
    () => wasmBackend.status,
    () => null,
  );
  const overlay = useMemo<Overlay | null>(() => {
    try {
      return status ? (JSON.parse(status).object?.overlay ?? null) : null;
    } catch {
      return null;
    }
  }, [status]);

  return (
    <>
      <video ref={video} autoPlay muted playsInline className="stream-image" />
      {overlay && (
        // Same aspect ratio and object-contain fit as the video, so frame pixels line up.
        <svg
          className="stream-image live-overlay"
          viewBox={`0 0 ${overlay.frame[0]} ${overlay.frame[1]}`}
          preserveAspectRatio="xMidYMid meet"
          aria-hidden="true"
        >
          <polygon points={overlay.box.map((p) => p.join(",")).join(" ")} className="overlay-halo" />
          <polygon points={overlay.box.map((p) => p.join(",")).join(" ")} className="overlay-box" />
          <polyline points={overlay.axis.map((p) => p.join(",")).join(" ")} className="overlay-halo" />
          <polyline points={overlay.axis.map((p) => p.join(",")).join(" ")} className="overlay-axis" />
          <circle cx={overlay.centroid[0]} cy={overlay.centroid[1]} r={4} className="overlay-dot" />
          <text x={overlay.centroid[0] + 12} y={overlay.centroid[1] - 12} className="overlay-text">
            {overlay.text}
          </text>
        </svg>
      )}
    </>
  );
}
