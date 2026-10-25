// Where browser-camera frames go (D-24). Locally that's server.exe over a WebSocket; the
// Netlify version (D-03) will implement the same interface with an in-browser WASM pipeline,
// so the capture side never changes.
export interface FrameSink {
  // Resolves once the sink is ready for the next frame. Awaiting it before sending again is
  // the flow control: never more than one frame in flight (H-50).
  send(jpeg: Blob): Promise<void>;
  close(): void;
}

export class SinkError extends Error {}

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
        send(jpeg) {
          if (pending) return Promise.reject(new SinkError("previous frame not acked yet"));
          if (ws.readyState !== WebSocket.OPEN)
            return Promise.reject(new SinkError("connection to server.exe closed"));
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
