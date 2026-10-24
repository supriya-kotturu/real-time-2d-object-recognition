import { Badge } from "@/components/ui/badge";
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from "@/components/ui/card";
import type { Status } from "@/lib/api";

export function PredictionCard({ status }: { status: Status | null }) {
  const object = status?.object ?? null;
  const prediction = object?.prediction ?? null;

  let body: React.ReactNode;
  if (!object) {
    body = <p className="empty-state">No object in view</p>;
  } else if (!prediction) {
    // featureStdevs needs two samples before any distance is meaningful.
    body = <p className="empty-state">Train at least 2 samples to classify</p>;
  } else {
    body = (
      <div className="prediction-body">
        <div className="prediction-row">
          <span className="prediction-label">
            {prediction.known ? prediction.label : "Unknown"}
          </span>
          <Badge variant={prediction.known ? "default" : "destructive"}>
            {prediction.known ? "known" : "unknown"}
          </Badge>
        </div>
        <p className="prediction-detail">
          nearest: {prediction.label} · distance {prediction.distance.toFixed(2)} / cutoff{" "}
          {status?.unknownThreshold.toFixed(1)}
        </p>
      </div>
    );
  }

  return (
    <Card>
      <CardHeader>
        <CardTitle>Prediction</CardTitle>
        <CardDescription>Nearest neighbour, scaled Euclidean</CardDescription>
      </CardHeader>
      <CardContent>{body}</CardContent>
    </Card>
  );
}
