import { Badge } from "@/components/ui/badge";
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from "@/components/ui/card";
import { formatDistance } from "@/components/settings-card";
import { CLASSIFIERS, metricInfo, type Status } from "@/lib/api";

export function PredictionCard({ status }: { status: Status | null }) {
  const object = status?.object ?? null;
  const prediction = object?.prediction ?? null;

  const metric = status?.metric ?? "scaled-euclidean";
  const method = CLASSIFIERS.find((c) => c.value === (status?.classifier ?? "features"))!.label;

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
          nearest: {prediction.label} · distance {formatDistance(prediction.distance, metric)} /
          cutoff {formatDistance(status?.unknownThreshold ?? 0, metric)}
        </p>
      </div>
    );
  }

  return (
    <Card>
      <CardHeader>
        <CardTitle>Prediction</CardTitle>
        <CardDescription>
          {method}, {metricInfo(metric).label.toLowerCase()}
        </CardDescription>
      </CardHeader>
      <CardContent>{body}</CardContent>
    </Card>
  );
}
