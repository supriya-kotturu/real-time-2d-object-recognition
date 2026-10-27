#!/usr/bin/env bash
# D-27: build the browser module and copy it into the web app. Run inside emscripten/emsdk:
#   docker run --rm -v objrec-wasm:/w -v "<repo>:/repo" emscripten/emsdk:3.1.74 bash /repo/wasm/build.sh
# (OpenCV must have been built once with wasm/build-opencv.sh into the same /w volume.)
set -euo pipefail

WORK=${WORK:-/w}
REPO=${REPO:-/repo}
[ -d "$WORK/opencv-wasm" ] || bash "$REPO/wasm/build-opencv.sh"

emcmake cmake -S "$REPO/wasm" -B "$WORK/build-objrec" -DCMAKE_BUILD_TYPE=Release \
    -DOPENCV_WASM="$WORK/opencv-wasm"
cmake --build "$WORK/build-objrec" -j"$(nproc)"

mkdir -p "$REPO/web/public/wasm"
cp "$WORK/build-objrec/objrec.mjs" "$WORK/build-objrec/objrec.wasm" "$REPO/web/public/wasm/"
ls -la "$REPO/web/public/wasm"
