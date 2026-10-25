"use client";

import { Camera, CameraOff, Images } from "lucide-react";

import { Badge } from "@/components/ui/badge";
import { Button } from "@/components/ui/button";
import type { useBrowserCamera } from "@/hooks/use-browser-camera";

type CameraState = ReturnType<typeof useBrowserCamera>;

const PHASE_LABEL = { idle: "camera off", starting: "starting…", live: "live", error: "error" };

export function CameraControls({ camera }: { camera: CameraState }) {
  const { phase, devices, deviceId, sentFps, kind, sampleLabel, start, startSamples, stop } =
    camera;
  const running = phase === "live" || phase === "starting";

  return (
    <div className="camera-controls">
      {devices.length > 1 && kind === "camera" && (
        // Native select: switching restarts getUserMedia with the chosen device.
        <select
          className="camera-select"
          value={deviceId}
          onChange={(e) => start(e.target.value)}
          aria-label="Camera"
        >
          {devices.map((d, i) => (
            <option key={d.deviceId} value={d.deviceId}>
              {d.label || `Camera ${i + 1}`}
            </option>
          ))}
        </select>
      )}
      {running ? (
        <Button variant="outline" size="sm" onClick={stop}>
          <CameraOff /> Stop
        </Button>
      ) : (
        <span className="camera-source">
          <Button size="sm" onClick={() => start(deviceId || undefined)}>
            <Camera /> Start camera
          </Button>
          <Button size="sm" variant="outline" onClick={startSamples}>
            <Images /> Sample images
          </Button>
        </span>
      )}
      <Badge variant={phase === "error" ? "destructive" : "outline"}>
        {phase === "live" ? `sending ${sentFps.toFixed(1)} fps` : PHASE_LABEL[phase]}
      </Badge>
      {sampleLabel && <Badge variant="secondary">true label: {sampleLabel}</Badge>}
    </div>
  );
}
