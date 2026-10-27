import type { Metadata } from "next";
import { Geist, Geist_Mono } from "next/font/google";

import { Toaster } from "@/components/ui/sonner";
import "./globals.css";

// shadcn's theme reads --font-sans, so Geist is registered under that name.
const geistSans = Geist({
  variable: "--font-sans",
  subsets: ["latin"],
});

const geistMono = Geist_Mono({
  variable: "--font-geist-mono",
  subsets: ["latin"],
});

const title = "Silhouette · real-time 2-D object recognition";
const description =
  "Teach your camera a new object with one click. Classical shape features vs ResNet18 embeddings, running entirely in your browser: C++/OpenCV compiled to WebAssembly, ResNet18 on WebGPU.";

// The image comes from opengraph-image.png next to this file; these fields shape the
// link preview when the URL is shared (LinkedIn, Slack, X).
export const metadata: Metadata = {
  metadataBase: new URL("https://real-time-2d-object-recognition.netlify.app"),
  title,
  description,
  authors: [{ name: "Supriya Kotturu", url: "https://www.linkedin.com/in/supriya-kotturu" }],
  alternates: { canonical: "/" },
  openGraph: { type: "website", url: "/", siteName: "Silhouette", title, description },
  twitter: { card: "summary_large_image", title, description },
};

export default function RootLayout({ children }: LayoutProps<"/">) {
  return (
    // suppressHydrationWarning: extensions like Grammarly inject attributes into <html>/<body>
    // before React hydrates. It only silences attribute diffs on these two elements.
    <html
      lang="en"
      // Dark by default (D-28): the lab-bench palette lives in the .dark tokens.
      className={`${geistSans.variable} ${geistMono.variable} app-root dark`}
      suppressHydrationWarning
    >
      <body className="app-body" suppressHydrationWarning>
        {children}
        <Toaster position="bottom-right" theme="dark" />
      </body>
    </html>
  );
}
