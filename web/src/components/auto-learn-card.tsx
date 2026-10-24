"use client";

import { Sparkles, X } from "lucide-react";
import { useState } from "react";
import { toast } from "sonner";

import { Button } from "@/components/ui/button";
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from "@/components/ui/card";
import { Input } from "@/components/ui/input";
import { Progress } from "@/components/ui/progress";
import { api, errorMessage, LABEL_PATTERN, type AutoLearnStatus } from "@/lib/api";

export function AutoLearnCard({ autoLearn }: { autoLearn: AutoLearnStatus | undefined }) {
  const [draft, setDraft] = useState("");
  const state = autoLearn?.state ?? "idle";
  const draftValid = LABEL_PATTERN.test(draft);

  const learn = async (e: React.FormEvent) => {
    e.preventDefault();
    if (!draftValid) return;
    try {
      await api.autoLearnLabel(draft);
      toast.success(`Learning "${draft}"…`);
      setDraft("");
    } catch (err) {
      toast.error(errorMessage(err));
    }
  };

  const dismiss = () => api.autoLearnDismiss().catch((err) => toast.error(errorMessage(err)));

  return (
    <Card className={state === "prompt" ? "autolearn-alert" : undefined}>
      <CardHeader>
        <CardTitle>Auto-learn</CardTitle>
        <CardDescription>Unknown objects held still for 3 s get added to the DB</CardDescription>
      </CardHeader>
      <CardContent className="control-stack">
        {state === "idle" && (
          <p className="empty-state">Show the camera an object it doesn&apos;t know yet</p>
        )}

        {state === "tracking" && (
          <Progress value={(autoLearn?.progress ?? 0) * 100}>
            <span className="progress-caption">Unknown object, hold still…</span>
          </Progress>
        )}

        {state === "prompt" && (
          <form onSubmit={learn} className="control-stack">
            <p className="autolearn-question">What is this object?</p>
            <Input
              autoFocus
              placeholder="e.g. stapler"
              value={draft}
              onChange={(e) => setDraft(e.target.value)}
              aria-invalid={draft !== "" && !draftValid}
            />
            <div className="control-actions">
              <Button type="submit" disabled={!draftValid}>
                <Sparkles /> Learn
              </Button>
              <Button type="button" variant="outline" onClick={dismiss}>
                <X /> Ignore
              </Button>
            </div>
          </form>
        )}

        {state === "collecting" && autoLearn && (
          <Progress value={(autoLearn.collected / autoLearn.target) * 100}>
            <span className="progress-caption">
              Learning {autoLearn.label}: {autoLearn.collected}/{autoLearn.target} samples
            </span>
          </Progress>
        )}
      </CardContent>
    </Card>
  );
}
