"use client";

import { Camera, CameraOff } from "lucide-react";

import { Badge } from "@/components/ui/badge";
import { Button } from "@/components/ui/button";
import type { useBrowserCamera } from "@/hooks/use-browser-camera";

type CameraState = ReturnType<typeof useBrowserCamera>;

const PHASE_LABEL = { idle: "camera off", starting: "starting…", live: "live", error: "error" };

export function CameraControls({ camera }: { camera: CameraState }) {
  const { phase, devices, deviceId, sentFps, start, stop } = camera;
  const running = phase === "live" || phase === "starting";

  return (
    <div className="camera-controls">
      {devices.length > 1 && (
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
          <CameraOff /> Stop camera
        </Button>
      ) : (
        <Button size="sm" onClick={() => start(deviceId || undefined)}>
          <Camera /> Start camera
        </Button>
      )}
      <Badge variant={phase === "error" ? "destructive" : "outline"}>
        {phase === "live" ? `sending ${sentFps.toFixed(1)} fps` : PHASE_LABEL[phase]}
      </Badge>
    </div>
  );
}
