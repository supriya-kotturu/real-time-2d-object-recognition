#!/usr/bin/env bash
# D-27: OpenCV 5 as static WebAssembly libraries, only the modules the pipeline links.
# Runs inside emscripten/emsdk (see wasm/README.md); flags follow OpenCV's own
# platforms/js/build_js.py, minus the JS bindings (we export our own C API instead).
set -euo pipefail

OPENCV_TAG=${OPENCV_TAG:-5.0.0}
WORK=${WORK:-/w}
cd "$WORK"
[ -d opencv ] || git clone -q --depth 1 --branch "$OPENCV_TAG" https://github.com/opencv/opencv

# SIMD128 runs in every current browser; exceptions on because CV_Assert throws.
FLAGS="-msimd128 -fexceptions"

emcmake cmake -S opencv -B build-opencv \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$WORK/opencv-wasm" \
    -DBUILD_LIST=core,imgproc,geometry \
    -DBUILD_SHARED_LIBS=OFF -DENABLE_PIC=FALSE \
    -DCPU_BASELINE='' -DCPU_DISPATCH='' -DCV_ENABLE_INTRINSICS=ON \
    -DCV_TRACE=OFF -DWITH_PTHREADS_PF=OFF -DBUILD_ZLIB=ON \
    -DWITH_ADE=OFF -DWITH_VTK=OFF -DWITH_EIGEN=OFF -DWITH_FFMPEG=OFF -DWITH_GSTREAMER=OFF \
    -DWITH_GTK=OFF -DWITH_IPP=OFF -DWITH_ITT=OFF -DWITH_TBB=OFF -DWITH_OPENCL=OFF \
    -DWITH_OPENGL=OFF -DWITH_LAPACK=OFF -DWITH_PNG=OFF -DWITH_JPEG=OFF -DWITH_TIFF=OFF \
    -DWITH_WEBP=OFF -DWITH_OPENEXR=OFF -DWITH_OPENJPEG=OFF -DWITH_JASPER=OFF -DWITH_AVIF=OFF \
    -DBUILD_TESTS=OFF -DBUILD_PERF_TESTS=OFF -DBUILD_EXAMPLES=OFF -DBUILD_DOCS=OFF \
    -DBUILD_opencv_apps=OFF -DBUILD_PACKAGE=OFF -DBUILD_opencv_js=OFF \
    -DCMAKE_C_FLAGS="$FLAGS" -DCMAKE_CXX_FLAGS="$FLAGS"

cmake --build build-opencv -j"$(nproc)" --target install
echo "OpenCV wasm installed in $WORK/opencv-wasm"
