// D-27: run Next for the browser build (pipeline in WebAssembly, no server).
//   node scripts/wasm.mjs dev    -> dev server on 127.0.0.1
//   node scripts/wasm.mjs build  -> static export in out/ (what Netlify publishes)
// Copies the committed feature/embedding DBs and onnxruntime-web's runtime into public/, and
// sets the env here rather than in package.json, where VAR=x syntax breaks on Windows cmd.
import { spawnSync } from "node:child_process";
import { copyFileSync, mkdirSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const web = join(dirname(fileURLToPath(import.meta.url)), "..");
const mode = process.argv[2];
if (mode !== "dev" && mode !== "build") {
  console.error("usage: node scripts/wasm.mjs dev|build");
  process.exit(2);
}

const copies = [
  ["../data/db/features.csv", "public/db/features.csv"],
  ["../data/db/embeddings_resnet18.csv", "public/db/embeddings_resnet18.csv"],
  // The default onnxruntime-web entry loads the JSEP (WebGPU-capable) build of its runtime,
  // even when only the wasm execution provider is used.
  ["node_modules/onnxruntime-web/dist/ort-wasm-simd-threaded.jsep.mjs", "public/ort/ort-wasm-simd-threaded.jsep.mjs"],
  ["node_modules/onnxruntime-web/dist/ort-wasm-simd-threaded.jsep.wasm", "public/ort/ort-wasm-simd-threaded.jsep.wasm"],
];
for (const [from, to] of copies) {
  mkdirSync(dirname(join(web, to)), { recursive: true });
  copyFileSync(join(web, from), join(web, to));
}

const env = { ...process.env, NEXT_PUBLIC_BACKEND: "wasm" };
if (mode === "build") env.OBJREC_EXPORT = "1";
const args = mode === "dev" ? ["dev", "-H", "127.0.0.1"] : ["build"];
const result = spawnSync(process.execPath, [join(web, "node_modules/next/dist/bin/next"), ...args], {
  cwd: web,
  env,
  stdio: "inherit",
});
process.exit(result.status ?? 1);
