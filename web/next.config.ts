import type { NextConfig } from "next";

// server.exe (D-13) listens on loopback only. API calls go through this rewrite so the
// browser stays same-origin; the MJPEG stream bypasses it (see src/lib/api.ts).
const backend = process.env.OBJREC_BACKEND ?? "http://127.0.0.1:8080";

const nextConfig: NextConfig = {
  cacheComponents: true,
  partialPrefetching: true,
  turbopack: {
    rules: {
      "*.css": {
        loaders: ["@tailwindcss/turbopack"],
        as: "*.css",
      },
    },
  },
  async rewrites() {
    return [{ source: "/api/:path*", destination: `${backend}/api/:path*` }];
  },
};

export default nextConfig;
