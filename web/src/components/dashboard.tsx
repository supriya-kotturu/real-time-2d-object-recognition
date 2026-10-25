"use client";

import { toast } from "sonner";

import { AutoLearnCard } from "@/components/auto-learn-card";
import { DatabaseCard } from "@/components/database-card";
import { EvaluateCard } from "@/components/evaluate-card";
import { FeaturesCard } from "@/components/features-card";
import { GalleryCard } from "@/components/gallery-card";
import { Logo } from "@/components/logo";
import { PredictionCard } from "@/components/prediction-card";
import { SettingsCard } from "@/components/settings-card";
import { StreamViewer } from "@/components/stream-viewer";
import { TrainingControls } from "@/components/training-controls";
import { Badge } from "@/components/ui/badge";
import { Tabs, TabsList, TabsTrigger } from "@/components/ui/tabs";
import { useDb } from "@/hooks/use-db";
import { useStatus } from "@/hooks/use-status";
import { api, errorMessage, MODES, type Mode } from "@/lib/api";
import { USE_WASM } from "@/lib/wasm-backend";

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
        <div className="brand">
          <Logo className="brand-logo" />
          <div>
            <h1 className="dashboard-title">Silhouette</h1>
            <p className="dashboard-subtitle">
              See the shape, name the thing
              <span className="dashboard-pipeline">
                {" "}
                · threshold → clean → regions → features → nearest neighbour
              </span>
            </p>
          </div>
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
        {/* Three areas so a phone stacks camera → prediction/controls → feature table: the answer
            sits right under the camera. On wide screens the side column spans both rows. */}
        <section className="dashboard-column dashboard-main">
          <StreamViewer status={status} connected={connected} />
        </section>
        <aside className="dashboard-column dashboard-side">
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
              {USE_WASM && <GalleryCard dbSize={status?.dbSize} />}
              <DatabaseCard db={db} onChanged={refresh} publicDemo={status?.public} />
            </>
          )}
          {mode === "evaluate" && (
            <>
              <PredictionCard status={status} />
              <SettingsCard status={status} />
              <EvaluateCard status={status} />
            </>
          )}
        </aside>
        <section className="dashboard-column dashboard-extra">
          <FeaturesCard object={status?.object ?? null} />
        </section>
      </div>
    </main>
  );
}
