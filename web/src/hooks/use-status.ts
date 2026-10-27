"use client";

import { useEffect, useMemo, useState, useSyncExternalStore } from "react";

import { api, type Status } from "@/lib/api";
import { USE_WASM, wasmBackend } from "@/lib/wasm-backend";

function usePolledStatus(intervalMs = 250) {
  const [status, setStatus] = useState<Status | null>(null);
  const [connected, setConnected] = useState(false);

  useEffect(() => {
    let cancelled = false;
    let timer: ReturnType<typeof setTimeout>;
    const controller = new AbortController();

    // Chained timeouts rather than setInterval, so a slow response never stacks requests.
    const tick = async () => {
      try {
        const next = await api.status(controller.signal);
        if (cancelled) return;
        setStatus(next);
        setConnected(true);
      } catch {
        if (cancelled) return;
        setConnected(false);
      }
      timer = setTimeout(tick, intervalMs);
    };
    tick();

    return () => {
      cancelled = true;
      controller.abort();
      clearTimeout(timer);
    };
  }, [intervalMs]);

  return { status, connected };
}

// The browser build pushes a status with every processed frame. Polling it lagged the cards
// behind the live overlay (which subscribes) by up to one interval, so on a sample switch the
// prediction card still named the previous object while the box already showed the new one.
const subscribe = (onChange: () => void) => wasmBackend.subscribe(onChange);

function useWasmStatus() {
  useEffect(() => wasmBackend.start(), []);
  const raw = useSyncExternalStore(
    subscribe,
    () => wasmBackend.status,
    () => null,
  );
  const fatal = useSyncExternalStore(
    subscribe,
    () => wasmBackend.fatal,
    () => null,
  );
  const status = useMemo(() => (raw ? (JSON.parse(raw) as Status) : null), [raw]);
  return { status, connected: status !== null && fatal === null };
}

export const useStatus = USE_WASM ? useWasmStatus : usePolledStatus;
