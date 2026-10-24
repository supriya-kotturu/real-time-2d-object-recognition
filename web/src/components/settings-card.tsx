"use client";

import { useState } from "react";
import { toast } from "sonner";

import { Card, CardContent, CardDescription, CardHeader, CardTitle } from "@/components/ui/card";
import { Slider } from "@/components/ui/slider";
import { api, errorMessage, type Status } from "@/lib/api";

const first = (v: number | readonly number[]) => (Array.isArray(v) ? v[0] : (v as number));

export function SettingsCard({ status }: { status: Status | null }) {
  // Non-null only while dragging, so polling can't yank the thumb out from under the user.
  const [draft, setDraft] = useState<number | null>(null);
  const value = draft ?? status?.unknownThreshold ?? 3;

  const commit = async (next: number) => {
    try {
      await api.setUnknownThreshold(next);
    } catch (err) {
      toast.error(errorMessage(err));
    } finally {
      setDraft(null);
    }
  };

  return (
    <Card>
      <CardHeader>
        <CardTitle>Unknown-object cutoff</CardTitle>
        <CardDescription>Matches farther than this are labelled unknown</CardDescription>
      </CardHeader>
      <CardContent className="control-stack">
        <div className="setting-row">
          <span>Max distance</span>
          <span className="feature-value">{value.toFixed(1)}</span>
        </div>
        <Slider
          min={0.5}
          max={10}
          step={0.1}
          value={[value]}
          disabled={!status}
          onValueChange={(v) => setDraft(first(v))}
          onValueCommitted={(v) => commit(first(v))}
        />
      </CardContent>
    </Card>
  );
}
