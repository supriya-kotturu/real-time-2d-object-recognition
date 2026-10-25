import type { NextConfig } from "next";

// server.exe (D-13) listens on loopback only. API calls go through this rewrite so the
// browser stays same-origin; the MJPEG stream bypasses it (see src/lib/api.ts).
const backend = process.env.OBJREC_BACKEND ?? "http://127.0.0.1:8080";

// D-26: OBJREC_EXPORT=1 next build emits a static site into out/, which server.exe serves
// itself. Same origin then, so there is nothing to rewrite (and exports can't rewrite anyway),
// and an empty stream origin makes every stream/WebSocket URL relative to the page.
const staticExport = process.env.OBJREC_EXPORT === "1";

const nextConfig: NextConfig = {
  ...(staticExport && { output: "export", env: { NEXT_PUBLIC_STREAM_ORIGIN: "" } }),
  // Partial prerendering needs a Next server; the static export has none.
  cacheComponents: !staticExport,
  partialPrefetching: !staticExport,
  turbopack: {
    rules: {
      "*.css": {
        loaders: ["@tailwindcss/turbopack"],
        as: "*.css",
      },
    },
  },
  ...(!staticExport && {
    async rewrites() {
      return [{ source: "/api/:path*", destination: `${backend}/api/:path*` }];
    },
  }),
};

export default nextConfig;
