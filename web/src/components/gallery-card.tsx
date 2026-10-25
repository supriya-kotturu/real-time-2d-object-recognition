"use client";

import { Trash2, X } from "lucide-react";
import { useEffect, useState } from "react";
import { toast } from "sonner";

import { Button } from "@/components/ui/button";
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from "@/components/ui/card";
import { api, errorMessage, type Gallery } from "@/lib/api";

// D-28: the samples this visitor trained, stored in their browser so a refresh keeps them.
// Refetches whenever the DB size changes (Train, auto-learn, deletes elsewhere).
export function GalleryCard({ dbSize }: { dbSize: number | undefined }) {
  const [gallery, setGallery] = useState<Gallery | null>(null);

  useEffect(() => {
    let cancelled = false;
    api.gallery().then(
      (g) => !cancelled && setGallery(g),
      () => {},
    );
    return () => {
      cancelled = true;
    };
  }, [dbSize]);

  const run = async (op: () => Promise<Gallery>, done: string) => {
    try {
      setGallery(await op());
      toast.success(done);
    } catch (err) {
      toast.error(errorMessage(err));
    }
  };

  const items = gallery?.items ?? [];
  const clearAll = () => {
    if (!window.confirm(`Delete all ${items.length} saved samples from this browser?`)) return;
    void run(api.galleryClear, "Saved samples cleared");
  };

  return (
    <Card>
      <CardHeader>
        <CardTitle>Your samples</CardTitle>
        <CardDescription>
          {gallery?.available === false
            ? "This browser blocks storage (private mode?): samples last until the tab closes"
            : `${items.length} / ${gallery?.cap ?? 50} saved in this browser only`}
        </CardDescription>
      </CardHeader>
      <CardContent className="control-stack">
        {items.length === 0 ? (
          <p className="empty-state">
            Nothing saved yet. Anything you Train or auto-learn appears here and survives a refresh.
          </p>
        ) : (
          <ul className="gallery-grid">
            {items.map((item) => (
              <li key={item.id} className="gallery-item">
                {item.thumb ? (
                  // eslint-disable-next-line @next/next/no-img-element -- data: URL thumbnails from IndexedDB; nothing for next/image to optimise
                  <img src={item.thumb} alt={`${item.label} sample`} className="gallery-thumb" />
                ) : (
                  <div className="gallery-thumb" />
                )}
                <span className="gallery-caption">{item.label}</span>
                <Button
                  variant="secondary"
                  size="icon-xs"
                  className="gallery-remove"
                  aria-label={`Delete this ${item.label} sample`}
                  onClick={() => run(() => api.galleryDelete(item.id), `Deleted a "${item.label}" sample`)}
                >
                  <X />
                </Button>
              </li>
            ))}
          </ul>
        )}
        {items.length > 0 && (
          <Button variant="outline" size="sm" onClick={clearAll}>
            <Trash2 /> Clear all
          </Button>
        )}
      </CardContent>
    </Card>
  );
}
