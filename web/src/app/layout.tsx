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
  title: "Object Recognition",
  description: "Live control panel for the CS5330 Project 3 recognition pipeline",
};

export default function RootLayout({ children }: LayoutProps<"/">) {
  return (
    // suppressHydrationWarning: extensions like Grammarly inject attributes into <html>/<body>
    // before React hydrates. It only silences attribute diffs on these two elements.
    <html
      lang="en"
      className={`${geistSans.variable} ${geistMono.variable} app-root`}
      suppressHydrationWarning
    >
      <body className="app-body" suppressHydrationWarning>
        {children}
        <Toaster position="bottom-right" />
      </body>
    </html>
  );
}
