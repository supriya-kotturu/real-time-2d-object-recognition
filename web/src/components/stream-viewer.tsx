"use client";

import { useEffect, useRef, useState } from "react";

import { CameraControls } from "@/components/camera-controls";
import { Badge } from "@/components/ui/badge";
import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card";
import { Tabs, TabsList, TabsTrigger } from "@/components/ui/tabs";
import { useBrowserCamera } from "@/hooks/use-browser-camera";
import { STREAM_ORIGIN, streamUrl, VIEWS, type Status, type View } from "@/lib/api";
import { USE_WASM, wasmBackend } from "@/lib/wasm-backend";

interface StreamViewerProps {
  status: Status | null;
  connected: boolean;
}

// Browser build (D-27): the worker returns the selected view with every processed frame.
function WasmCanvas() {
  const ref = useRef<HTMLCanvasElement>(null);
  useEffect(
    () =>
      wasmBackend.onView((view) => {
        const canvas = ref.current;
        if (!canvas) return view.close();
        canvas.width = view.width;
        canvas.height = view.height;
        // Takes ownership of the bitmap: no copy, no close needed.
        canvas.getContext("bitmaprenderer")?.transferFromImageBitmap(view);
      }),
    [],
  );
  return <canvas ref={ref} className="stream-image" aria-label="processed camera view" />;
}

export function StreamViewer({ status, connected }: StreamViewerProps) {
  const [view, setView] = useState<View>("annotated");
  // Lives here, not in a mode panel, so switching Detect/Collect/Evaluate keeps the camera on.
  const camera = useBrowserCamera();
  const browserSource = connected && status?.browser;

  const selectView = (next: View) => {
    setView(next);
    wasmBackend.view = next; // applies from the next frame
  };

  let overlay: string | null = null;
  if (!connected)
    overlay = USE_WASM
      ? (wasmBackend.fatal ?? "Loading the recognition pipeline (C++ compiled to WebAssembly)…")
      : `Waiting for server.exe${STREAM_ORIGIN ? ` on ${STREAM_ORIGIN}` : ""}…`;
  else if (browserSource && camera.phase === "error") overlay = camera.error;
  else if (browserSource && camera.phase !== "live")
    overlay =
      "Start camera (your browser asks for permission; point it down at the paper), or play the held-out sample images.";

  return (
    <Card>
      <CardHeader className="stream-header">
        <CardTitle>Camera</CardTitle>
        <Tabs value={view} onValueChange={(v) => selectView(v as View)}>
          <TabsList>
            {VIEWS.map((v) => (
              <TabsTrigger key={v.value} value={v.value}>
                {v.label}
              </TabsTrigger>
            ))}
          </TabsList>
        </Tabs>
      </CardHeader>
      <CardContent className="stream-body">
        {browserSource && <CameraControls camera={camera} />}
        <div className="stream-frame">
          {connected &&
            (USE_WASM ? (
              <WasmCanvas />
            ) : (
              // Keyed on view so switching tabs opens a fresh multipart connection; remounts
              // on reconnect because `connected` gates rendering.
              // eslint-disable-next-line @next/next/no-img-element -- MJPEG needs a raw <img>; next/image can't stream multipart
              <img key={view} src={streamUrl(view)} alt={`${view} view`} className="stream-image" />
            ))}
          {overlay && <div className="stream-overlay stream-overlay-solid">{overlay}</div>}
        </div>
        {status && (
          <div className="stream-meta">
            <Badge variant="outline">ISODATA t = {status.threshold}</Badge>
            <Badge variant="outline">{status.still ? "still image" : "live"}</Badge>
            <span>source: {status.source}</span>
          </div>
        )}
      </CardContent>
    </Card>
  );
}
