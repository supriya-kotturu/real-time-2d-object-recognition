"use client";

import { RotateCcw, Trash2 } from "lucide-react";
import { toast } from "sonner";

import { Button } from "@/components/ui/button";
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from "@/components/ui/card";
import { Table, TableBody, TableCell, TableHead, TableHeader, TableRow } from "@/components/ui/table";
import { api, errorMessage, type DbSummary } from "@/lib/api";

interface DatabaseCardProps {
  db: DbSummary | null;
  onChanged: () => void;
  publicDemo?: boolean;
}

export function DatabaseCard({ db, onChanged, publicDemo }: DatabaseCardProps) {
  // Public demo only: visitors' changes live in memory, and this restores the committed DB.
  const reset = async () => {
    try {
      await api.resetDb();
      onChanged();
      toast.success("Database reset to the original training set");
    } catch (err) {
      toast.error(errorMessage(err));
    }
  };

  const remove = async (label: string, count: number) => {
    if (!window.confirm(`Delete all ${count} "${label}" samples?`)) return;
    try {
      await api.deleteLabel(label);
      onChanged();
      toast.success(`Deleted "${label}"`);
    } catch (err) {
      toast.error(errorMessage(err));
    }
  };

  return (
    <Card>
      <CardHeader>
        <CardTitle>Object database</CardTitle>
        <CardDescription>{db ? `${db.total} samples` : "Loading…"}</CardDescription>
      </CardHeader>
      <CardContent>
        {db && db.labels.length > 0 ? (
          <Table>
            <TableHeader>
              <TableRow>
                <TableHead>Label</TableHead>
                <TableHead className="feature-value">Samples</TableHead>
                <TableHead className="w-10" />
              </TableRow>
            </TableHeader>
            <TableBody>
              {db.labels.map(({ label, count }) => (
                <TableRow key={label}>
                  <TableCell className="font-medium">{label}</TableCell>
                  <TableCell className="feature-value">{count}</TableCell>
                  <TableCell>
                    <Button
                      variant="ghost"
                      size="icon"
                      aria-label={`Delete ${label}`}
                      onClick={() => remove(label, count)}
                    >
                      <Trash2 />
                    </Button>
                  </TableCell>
                </TableRow>
              ))}
            </TableBody>
          </Table>
        ) : (
          <p className="empty-state">No samples yet: set a label and press Train</p>
        )}
        {publicDemo && (
          <Button variant="outline" size="sm" className="mt-3" onClick={reset}>
            <RotateCcw /> Reset DB
          </Button>
        )}
      </CardContent>
    </Card>
  );
}
