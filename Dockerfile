# syntax=docker/dockerfile:1
# D-26: hosted demo (Hugging Face Spaces, Docker SDK). server.exe --public serves the static GUI,
# the API and the camera WebSocket on one port. Multi-stage so the final image carries only the
# runtime: OpenCV's shared libs, the server binary, the exported GUI, the DBs and the model.

ARG UBUNTU=ubuntu:24.04

# --- OpenCV 5 from source: no distro ships it yet. Only the modules server links, and the
# image codecs bundled (BUILD_JPEG/PNG/ZLIB) so the runtime image needs no -dev packages.
FROM ${UBUNTU} AS opencv
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential cmake git ca-certificates \
    && rm -rf /var/lib/apt/lists/*
ARG OPENCV_TAG=5.0.0
RUN git clone --depth 1 --branch ${OPENCV_TAG} https://github.com/opencv/opencv /src/opencv
RUN cmake -S /src/opencv -B /build/opencv \
        -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/opt/opencv \
        -DBUILD_LIST=core,imgproc,imgcodecs,videoio,dnn,geometry \
        -DBUILD_TESTS=OFF -DBUILD_PERF_TESTS=OFF -DBUILD_EXAMPLES=OFF -DBUILD_DOCS=OFF \
        -DBUILD_opencv_apps=OFF -DBUILD_JAVA=OFF -DBUILD_opencv_python3=OFF \
        -DBUILD_JPEG=ON -DBUILD_PNG=ON -DBUILD_ZLIB=ON \
        -DWITH_FFMPEG=OFF -DWITH_GSTREAMER=OFF -DWITH_V4L=OFF -DWITH_GTK=OFF -DWITH_QT=OFF \
        -DWITH_OPENCL=OFF -DWITH_IPP=OFF -DWITH_ITT=OFF -DWITH_TIFF=OFF -DWITH_WEBP=OFF \
        -DWITH_OPENJPEG=OFF -DWITH_JASPER=OFF -DWITH_OPENEXR=OFF -DWITH_AVIF=OFF \
    && cmake --build /build/opencv -j"$(nproc)" --target install

# --- server binary
FROM opencv AS server
COPY CMakeLists.txt /src/app/
COPY include /src/app/include
COPY src /src/app/src
COPY tests /src/app/tests
COPY third_party /src/app/third_party
RUN cmake -S /src/app -B /build/app -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/opt/opencv \
    && cmake --build /build/app --target server -j"$(nproc)"

# --- static GUI (same-origin URLs; see web/next.config.ts)
FROM node:22-slim AS web
WORKDIR /web
COPY web/package.json web/package-lock.json ./
RUN npm ci
COPY web/ ./
RUN OBJREC_EXPORT=1 npx next build

# --- ResNet18, checked against the hash in data/models/README.md (D-18, D-19)
FROM ${UBUNTU} AS model
RUN apt-get update && apt-get install -y --no-install-recommends curl ca-certificates \
    && rm -rf /var/lib/apt/lists/*
RUN curl -fsSL -o /resnet18-v2-7.onnx \
        https://github.com/onnx/models/raw/main/validated/vision/classification/resnet/model/resnet18-v2-7.onnx \
    && echo "3b4d048057854022232aa20bf68262e31718ab7119a17f920b8a2dc1433f824b  /resnet18-v2-7.onnx" \
        | sha256sum -c -

# --- runtime
FROM ${UBUNTU}
COPY --from=opencv /opt/opencv/lib /opt/opencv/lib
ENV LD_LIBRARY_PATH=/opt/opencv/lib
WORKDIR /app
COPY --from=server /src/app/out/server out/server
COPY --from=web /web/out web/out
COPY --from=model /resnet18-v2-7.onnx data/models/resnet18-v2-7.onnx
COPY data/db data/db
# Spaces run the container as uid 1000; --public never writes, so read-only files are fine.
USER 1000
EXPOSE 7860
# The Netlify-hosted GUI (D-26) talks to this server cross-origin.
CMD ["out/server", "--public", "--allow-origin", "real-time-2d-object-recognition.netlify.app", "--browser", "7860"]
