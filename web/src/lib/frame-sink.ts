// Where browser-camera frames go (D-24). Locally that's server.exe over a WebSocket; in the
// Netlify build (D-27) it's the pipeline worker running our C++ as WebAssembly. The capture
// side is identical for both.

import { wasmBackend } from "@/lib/wasm-backend";

export interface FrameSink {
  // Takes ownership of the frame (it is closed or transferred). Resolves once the sink is ready
  // for the next one: awaiting it before sending again is the flow control, never more than
  // one frame in flight (H-50).
  send(frame: ImageBitmap): Promise<void>;
  close(): void;
}

export class SinkError extends Error {}

const JPEG_QUALITY = 0.8;

export function connectWasmSink(): Promise<FrameSink> {
  return Promise.resolve({
    send: (frame) => wasmBackend.sendFrame(frame),
    close() {},
  });
}

async function toJpeg(frame: ImageBitmap): Promise<Blob> {
  const canvas = new OffscreenCanvas(frame.width, frame.height);
  canvas.getContext("2d")!.drawImage(frame, 0, 0);
  frame.close();
  return canvas.convertToBlob({ type: "image/jpeg", quality: JPEG_QUALITY });
}

export function connectServerSink(url: string): Promise<FrameSink> {
  return new Promise((resolve, reject) => {
    const ws = new WebSocket(url);
    let pending: { resolve: () => void; reject: (err: Error) => void } | null = null;

    const fail = (err: Error) => {
      pending?.reject(err);
      pending = null;
    };

    ws.onmessage = (event) => {
      const reply = pending;
      pending = null;
      if (!reply) return;
      // The server answers every frame: {"ok":true} or {"error":"..."}.
      try {
        const body = JSON.parse(String(event.data));
        if (body.ok) reply.resolve();
        else reply.reject(new SinkError(body.error ?? "frame rejected"));
      } catch {
        reply.reject(new SinkError("unreadable reply from server"));
      }
    };

    ws.onerror = () => reject(new SinkError("cannot reach server.exe"));
    ws.onclose = () => fail(new SinkError("connection to server.exe closed"));

    ws.onopen = () => {
      // Opened: a later error only fails the frame in flight, not the connect promise.
      ws.onerror = null;
      resolve({
        async send(frame) {
          if (pending) throw new SinkError("previous frame not acked yet");
          const jpeg = await toJpeg(frame);
          if (ws.readyState !== WebSocket.OPEN)
            throw new SinkError("connection to server.exe closed");
          return new Promise<void>((res, rej) => {
            pending = { resolve: res, reject: rej };
            ws.send(jpeg);
          });
        },
        close() {
          ws.close();
        },
      });
    };
  });
}
