"use client";

import { useSyncExternalStore } from "react";

import { wasmBackend } from "@/lib/wasm-backend";
import type { ModelState } from "@/worker/protocol";

const serverSnapshot: ModelState = { state: "loading", progress: 0 };

// ResNet18's download/compile state in the browser build (D-27).
export function useWasmModel(): ModelState {
  return useSyncExternalStore(
    (onChange) => wasmBackend.subscribe(onChange),
    () => wasmBackend.model,
    () => serverSnapshot,
  );
}
