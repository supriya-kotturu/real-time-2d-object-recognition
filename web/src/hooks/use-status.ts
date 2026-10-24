"use client";

import { useEffect, useState } from "react";

import { api, type Status } from "@/lib/api";

export function useStatus(intervalMs = 250) {
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
