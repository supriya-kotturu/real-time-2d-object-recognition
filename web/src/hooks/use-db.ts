"use client";

import { useCallback, useEffect, useState } from "react";

import { api, type DbSummary } from "@/lib/api";

// Refetches whenever the server's sample count changes, so training from the C++ `live`
// window or another tab shows up here too.
export function useDb(dbSize: number | undefined) {
  const [db, setDb] = useState<DbSummary | null>(null);

  // Connection state is reported by useStatus, so failures just keep the last known table.
  const refresh = useCallback(() => {
    api.db().then(setDb, () => {});
  }, []);

  useEffect(() => {
    if (dbSize === undefined) return;
    let cancelled = false;
    api.db().then(
      (next) => {
        if (!cancelled) setDb(next);
      },
      () => {},
    );
    return () => {
      cancelled = true;
    };
  }, [dbSize]);

  return { db, refresh };
}
