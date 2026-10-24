"use client";

import { Camera, GraduationCap, SkipForward, Tag } from "lucide-react";
import { useState } from "react";
import { toast } from "sonner";

import { Button } from "@/components/ui/button";
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from "@/components/ui/card";
import { Input } from "@/components/ui/input";
import { Label } from "@/components/ui/label";
import { api, errorMessage, LABEL_PATTERN, type Status } from "@/lib/api";

interface TrainingControlsProps {
  status: Status | null;
  onTrained: () => void;
}

export function TrainingControls({ status, onTrained }: TrainingControlsProps) {
  const [draft, setDraft] = useState("");
  const [busy, setBusy] = useState(false);

  const draftValid = LABEL_PATTERN.test(draft);
  const hasLabel = Boolean(status?.label);
  const hasObject = Boolean(status?.object);

  const run = async (action: () => Promise<string>) => {
    setBusy(true);
    try {
      toast.success(await action());
    } catch (err) {
      toast.error(errorMessage(err));
    } finally {
      setBusy(false);
    }
  };

  const submitLabel = (e: React.FormEvent) => {
    e.preventDefault();
    if (!draftValid) return;
    run(async () => {
      const { label } = await api.setLabel(draft);
      setDraft("");
      return `Label set to "${label}"`;
    });
  };

  return (
    <Card>
      <CardHeader>
        <CardTitle>Training</CardTitle>
        <CardDescription>
          Current label: <span className="font-medium text-foreground">{status?.label || "none"}</span>
        </CardDescription>
      </CardHeader>
      <CardContent className="control-stack">
        <form onSubmit={submitLabel} className="control-row">
          <div className="control-field">
            <Label htmlFor="label-input">Object label</Label>
            <Input
              id="label-input"
              placeholder="e.g. scissors"
              value={draft}
              onChange={(e) => setDraft(e.target.value)}
              aria-invalid={draft !== "" && !draftValid}
            />
          </div>
          <Button type="submit" variant="secondary" disabled={!draftValid || busy}>
            <Tag /> Set
          </Button>
        </form>
        {draft !== "" && !draftValid && (
          <p className="control-hint">Letters, digits, _ and - only (max 64)</p>
        )}
        <div className="control-actions">
          <Button
            disabled={!hasLabel || !hasObject || busy}
            onClick={() =>
              run(async () => {
                const db = await api.train();
                onTrained();
                return `Trained "${status?.label}" (${db.total} samples)`;
              })
            }
          >
            <GraduationCap /> Train
          </Button>
          <Button
            variant="outline"
            disabled={!hasLabel || busy}
            onClick={() => run(async () => `Saved ${(await api.capture()).saved}`)}
          >
            <Camera /> Capture
          </Button>
        </div>
        {status?.still && (
          <Button
            variant="ghost"
            disabled={busy}
            onClick={() =>
              run(async () => {
                await api.next();
                return "Next image";
              })
            }
          >
            <SkipForward /> Next image
          </Button>
        )}
      </CardContent>
    </Card>
  );
}
