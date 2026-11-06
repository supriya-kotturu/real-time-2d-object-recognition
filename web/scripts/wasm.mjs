// D-27: run Next for the browser build (pipeline in WebAssembly, no server).
//   node scripts/wasm.mjs dev    -> dev server on 127.0.0.1
//   node scripts/wasm.mjs build  -> static export in out/ (what Netlify publishes)
// Copies the committed feature/embedding DBs, onnxruntime-web's runtime and ResNet18 into
// public/, and sets the env here rather than in package.json, where VAR=x syntax breaks on
// Windows cmd.
import { spawnSync } from "node:child_process";
import { createHash } from "node:crypto";
import { copyFileSync, existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
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

// D-30: the site serves ResNet18 itself; some networks block the zoo's LFS host, which the
// browser used to fetch at runtime. Weights stay out of git (D-18): reuse data/models/ when its
// copy matches the pin, otherwise download at build time (server-side, so no CORS involved).
const MODEL_SHA256 = "3b4d048057854022232aa20bf68262e31718ab7119a17f920b8a2dc1433f824b";
const MODEL_SOURCE =
  "https://github.com/onnx/models/raw/main/validated/vision/classification/resnet/model/resnet18-v2-7.onnx";
const localModel = join(web, "../data/models/resnet18-v2-7.onnx");
const sha256 = (bytes) => createHash("sha256").update(bytes).digest("hex");
let model = existsSync(localModel) ? readFileSync(localModel) : null;
if (!model || sha256(model) !== MODEL_SHA256) {
  console.log("downloading ResNet18 from the ONNX model zoo (47 MB)");
  const res = await fetch(MODEL_SOURCE);
  if (!res.ok) throw new Error(`ResNet18 download failed: HTTP ${res.status}`);
  model = Buffer.from(await res.arrayBuffer());
  if (sha256(model) !== MODEL_SHA256) throw new Error("ResNet18 download does not match the pinned SHA-256");
  mkdirSync(dirname(localModel), { recursive: true });
  writeFileSync(localModel, model);
}
mkdirSync(join(web, "public/models"), { recursive: true });
writeFileSync(join(web, "public/models/resnet18-v2-7.onnx"), model);

const env = { ...process.env, NEXT_PUBLIC_BACKEND: "wasm" };
if (mode === "build") env.OBJREC_EXPORT = "1";
const args = mode === "dev" ? ["dev", "-H", "127.0.0.1"] : ["build"];
const result = spawnSync(process.execPath, [join(web, "node_modules/next/dist/bin/next"), ...args], {
  cwd: web,
  env,
  stdio: "inherit",
});
process.exit(result.status ?? 1);
