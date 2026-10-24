"use client";

import { ClipboardCheck, RotateCcw, Tag } from "lucide-react";
import { useEffect, useState } from "react";
import { toast } from "sonner";

import { Button } from "@/components/ui/button";
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from "@/components/ui/card";
import { Input } from "@/components/ui/input";
import { Table, TableBody, TableCell, TableHead, TableHeader, TableRow } from "@/components/ui/table";
import { api, errorMessage, LABEL_PATTERN, type EvalMatrix, type Status } from "@/lib/api";

// T7 hook: state the true label, then record what the system predicts for it.
export function EvaluateCard({ status }: { status: Status | null }) {
  const [draft, setDraft] = useState("");
  const [matrix, setMatrix] = useState<EvalMatrix | null>(null);
  const draftValid = LABEL_PATTERN.test(draft);

  useEffect(() => {
    api.evalMatrix().then(setMatrix, () => {});
  }, []);

  const act = async (fn: () => Promise<EvalMatrix>, message: string) => {
    try {
      setMatrix(await fn());
      toast.success(message);
    } catch (err) {
      toast.error(errorMessage(err));
    }
  };

  const setTruth = async (e: React.FormEvent) => {
    e.preventDefault();
    if (!draftValid) return;
    try {
      await api.setLabel(draft);
      setDraft("");
    } catch (err) {
      toast.error(errorMessage(err));
    }
  };

  return (
    <Card>
      <CardHeader>
        <CardTitle>Evaluate</CardTitle>
        <CardDescription>
          True label: <span className="font-medium text-foreground">{status?.label || "none"}</span>
        </CardDescription>
      </CardHeader>
      <CardContent className="control-stack">
        <form onSubmit={setTruth} className="control-row">
          <Input
            placeholder="true label, e.g. scissors"
            value={draft}
            onChange={(e) => setDraft(e.target.value)}
            aria-invalid={draft !== "" && !draftValid}
          />
          <Button type="submit" variant="secondary" disabled={!draftValid}>
            <Tag /> Set
          </Button>
        </form>
        <div className="control-actions">
          <Button
            disabled={!status?.label || !status?.object}
            onClick={() => act(api.evalRecord, "Recorded")}
          >
            <ClipboardCheck /> Record
          </Button>
          <Button variant="outline" onClick={() => act(api.evalReset, "Matrix reset")}>
            <RotateCcw /> Reset
          </Button>
        </div>

        {matrix && matrix.total > 0 ? (
          <>
            <Table>
              <TableHeader>
                <TableRow>
                  <TableHead>true \ pred</TableHead>
                  {matrix.columns.map((c) => (
                    <TableHead key={c} className="feature-value">
                      {c}
                    </TableHead>
                  ))}
                </TableRow>
              </TableHeader>
              <TableBody>
                {matrix.rows.map((row, r) => (
                  <TableRow key={row}>
                    <TableCell className="font-medium">{row}</TableCell>
                    {matrix.columns.map((col, c) => (
                      <TableCell
                        key={col}
                        className={row === col ? "matrix-hit" : "feature-value"}
                      >
                        {matrix.counts[r][c]}
                      </TableCell>
                    ))}
                  </TableRow>
                ))}
              </TableBody>
            </Table>
            <p className="prediction-detail">
              Accuracy {(matrix.accuracy * 100).toFixed(1)}% over {matrix.total} records · saved
              to out/eval/live_confusion.md
            </p>
          </>
        ) : (
          <p className="empty-state">Set a true label, show the object, press Record</p>
        )}
      </CardContent>
    </Card>
  );
}
