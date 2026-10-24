"use client";

import { Badge } from "@/components/ui/badge";
import { DatabaseCard } from "@/components/database-card";
import { FeaturesCard } from "@/components/features-card";
import { PredictionCard } from "@/components/prediction-card";
import { SettingsCard } from "@/components/settings-card";
import { StreamViewer } from "@/components/stream-viewer";
import { TrainingControls } from "@/components/training-controls";
import { useDb } from "@/hooks/use-db";
import { useStatus } from "@/hooks/use-status";

export function Dashboard() {
  const { status, connected } = useStatus();
  const { db, refresh } = useDb(status?.dbSize);

  return (
    <main className="dashboard-shell">
      <header className="dashboard-header">
        <div>
          <h1 className="dashboard-title">Real-time 2-D Object Recognition</h1>
          <p className="dashboard-subtitle">Threshold → clean → regions → features → nearest neighbour</p>
        </div>
        <Badge variant={connected ? "secondary" : "destructive"}>
          {connected ? `connected · ${status?.fps.toFixed(1)} fps` : "server offline"}
        </Badge>
      </header>

      <div className="dashboard-grid">
        <section className="dashboard-column">
          <StreamViewer status={status} connected={connected} />
          <FeaturesCard object={status?.object ?? null} />
        </section>
        <aside className="dashboard-column">
          <PredictionCard status={status} />
          <TrainingControls status={status} onTrained={refresh} />
          <SettingsCard status={status} />
          <DatabaseCard db={db} onChanged={refresh} />
        </aside>
      </div>
    </main>
  );
}
