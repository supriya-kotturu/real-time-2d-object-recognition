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

export const metadata: Metadata = {
  title: "Silhouette · real-time 2-D object recognition",
  description:
    "See the shape, name the thing: classical shape features vs ResNet18 embeddings, running entirely in your browser (CS5330 Project 3).",
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
