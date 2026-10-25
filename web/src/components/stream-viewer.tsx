"use client";

import { useState } from "react";

import { CameraControls } from "@/components/camera-controls";
import { Badge } from "@/components/ui/badge";
import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card";
import { Tabs, TabsList, TabsTrigger } from "@/components/ui/tabs";
import { useBrowserCamera } from "@/hooks/use-browser-camera";
import { streamUrl, VIEWS, type Status, type View } from "@/lib/api";

interface StreamViewerProps {
  status: Status | null;
  connected: boolean;
}

export function StreamViewer({ status, connected }: StreamViewerProps) {
  const [view, setView] = useState<View>("annotated");
  // Lives here, not in a mode panel, so switching Detect/Collect/Evaluate keeps the camera on.
  const camera = useBrowserCamera();
  const browserSource = connected && status?.browser;

  let overlay: string | null = null;
  if (!connected) overlay = "Waiting for server.exe on 127.0.0.1:8080…";
  else if (browserSource && camera.phase === "error") overlay = camera.error;
  else if (browserSource && camera.phase !== "live")
    overlay = "Press Start camera. Your browser will ask for permission; point the camera down at the paper.";

  return (
    <Card>
      <CardHeader className="stream-header">
        <CardTitle>Camera</CardTitle>
        <Tabs value={view} onValueChange={(v) => setView(v as View)}>
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
          {connected && (
            // Keyed on view so switching tabs opens a fresh multipart connection; remounts
            // on reconnect because `connected` gates rendering.
            // eslint-disable-next-line @next/next/no-img-element -- MJPEG needs a raw <img>; next/image can't stream multipart
            <img key={view} src={streamUrl(view)} alt={`${view} view`} className="stream-image" />
          )}
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
