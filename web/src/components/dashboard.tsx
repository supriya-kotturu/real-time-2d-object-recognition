"use client";

import { toast } from "sonner";

import { AutoLearnCard } from "@/components/auto-learn-card";
import { DatabaseCard } from "@/components/database-card";
import { EvaluateCard } from "@/components/evaluate-card";
import { FeaturesCard } from "@/components/features-card";
import { PredictionCard } from "@/components/prediction-card";
import { SettingsCard } from "@/components/settings-card";
import { StreamViewer } from "@/components/stream-viewer";
import { TrainingControls } from "@/components/training-controls";
import { Badge } from "@/components/ui/badge";
import { Tabs, TabsList, TabsTrigger } from "@/components/ui/tabs";
import { useDb } from "@/hooks/use-db";
import { useStatus } from "@/hooks/use-status";
import { api, errorMessage, MODES, type Mode } from "@/lib/api";

export function Dashboard() {
  const { status, connected } = useStatus();
  const { db, refresh } = useDb(status?.dbSize);
  // The server owns the mode so every open tab agrees; the next poll reflects a change.
  const mode = status?.mode ?? "detect";

  const changeMode = (next: Mode) =>
    api.setMode(next).catch((err) => toast.error(errorMessage(err)));

  return (
    <main className="dashboard-shell">
      <header className="dashboard-header">
        <div>
          <h1 className="dashboard-title">Real-time 2-D Object Recognition</h1>
          <p className="dashboard-subtitle">Threshold → clean → regions → features → nearest neighbour</p>
        </div>
        <div className="header-actions">
          <Tabs value={mode} onValueChange={(v) => changeMode(v as Mode)}>
            <TabsList>
              {MODES.map((m) => (
                <TabsTrigger key={m.value} value={m.value} disabled={!connected}>
                  {m.label}
                </TabsTrigger>
              ))}
            </TabsList>
          </Tabs>
          <Badge variant={connected ? "secondary" : "destructive"}>
            {connected ? `connected · ${status?.fps.toFixed(1)} fps` : "server offline"}
          </Badge>
        </div>
      </header>

      <div className="dashboard-grid">
        <section className="dashboard-column">
          <StreamViewer status={status} connected={connected} />
          <FeaturesCard object={status?.object ?? null} />
        </section>
        <aside className="dashboard-column">
          {mode === "detect" && (
            <>
              <PredictionCard status={status} />
              <AutoLearnCard autoLearn={status?.autoLearn} />
              <SettingsCard status={status} />
            </>
          )}
          {mode === "collect" && (
            <>
              <TrainingControls status={status} onTrained={refresh} />
              <DatabaseCard db={db} onChanged={refresh} />
            </>
          )}
          {mode === "evaluate" && (
            <>
              <PredictionCard status={status} />
              <EvaluateCard status={status} />
            </>
          )}
        </aside>
      </div>
    </main>
  );
}
