// Messages between the GUI and the pipeline worker (D-27). The worker plays the role server.exe
// plays locally: frames in, status and the selected view out, API commands by name.

export type ViewName = "annotated" | "threshold" | "regions";

export type ToWorker =
  | { type: "frame"; id: number; frame: ImageBitmap; view: ViewName }
  | { type: "command"; id: number; name: string; a?: string; b?: string; n?: number };

export type ModelState =
  | { state: "loading"; progress: number } // 0..1 of the 47 MB download
  | { state: "ready" }
  | { state: "error"; message: string };

export type FromWorker =
  | { type: "ready"; status: string }
  | { type: "fatal"; message: string }
  | { type: "frameDone"; id: number; status: string; view: ImageBitmap | null }
  | { type: "reply"; id: number; status: number; body: string }
  | { type: "model"; model: ModelState; status: string };
