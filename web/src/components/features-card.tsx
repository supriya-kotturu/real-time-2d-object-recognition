import { Card, CardContent, CardDescription, CardHeader, CardTitle } from "@/components/ui/card";
import { Table, TableBody, TableCell, TableHead, TableHeader, TableRow } from "@/components/ui/table";
import type { DetectedObject } from "@/lib/api";

// Mirrors the comments on kFeatureNames in include/core/features.h.
const DESCRIPTIONS: Record<string, string> = {
  percentFilled: "region area / oriented-box area",
  aspectRatio: "oriented box short side / long side",
  hu1: "spread of mass around the centroid",
  hu2: "elongation",
  hu3: "asymmetry",
};

export function FeaturesCard({ object }: { object: DetectedObject | null }) {
  return (
    <Card>
      <CardHeader>
        <CardTitle>Feature vector</CardTitle>
        <CardDescription>Rotate or move the object: these should barely change</CardDescription>
      </CardHeader>
      <CardContent>
        {object ? (
          <Table>
            <TableHeader>
              <TableRow>
                <TableHead>Feature</TableHead>
                <TableHead className="feature-meaning">Meaning</TableHead>
                <TableHead className="feature-value">Value</TableHead>
              </TableRow>
            </TableHeader>
            <TableBody>
              {Object.entries(object.features).map(([name, value]) => (
                <TableRow key={name}>
                  <TableCell className="font-medium">{name}</TableCell>
                  <TableCell className="feature-meaning text-muted-foreground">
                    {DESCRIPTIONS[name] ?? ""}
                  </TableCell>
                  <TableCell className="feature-value">{value.toPrecision(4)}</TableCell>
                </TableRow>
              ))}
            </TableBody>
          </Table>
        ) : (
          <p className="empty-state">No object in view</p>
        )}
      </CardContent>
    </Card>
  );
}
