# D-27: the pipeline as a WebAssembly module for the browser (Web Worker). Built with
# emcmake inside emscripten/emsdk by wasm/build.sh, against the static OpenCV from
# wasm/build-opencv.sh. Compiles the same core/app sources as the native build, minus
# everything that needs files, windows, cameras or OpenCV's dnn module.
cmake_minimum_required(VERSION 3.20)
project(objrec_wasm LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Linked directly: OpenCV's exported config also references a protobuf target that a
# core/imgproc/geometry-only build never installs, so find_package fails on it.
set(OPENCV_WASM "/w/opencv-wasm" CACHE PATH "install prefix from build-opencv.sh")

set(ROOT "${CMAKE_CURRENT_SOURCE_DIR}/..")
add_executable(objrec
	objrec_wasm.cpp
	${ROOT}/src/core/threshold.cpp
	${ROOT}/src/core/morphology.cpp
	${ROOT}/src/core/regions.cpp
	${ROOT}/src/core/features.cpp
	${ROOT}/src/core/classifier.cpp
	${ROOT}/src/core/evaluation.cpp
	${ROOT}/src/core/preprocess.cpp
	${ROOT}/src/app/pipeline.cpp
	${ROOT}/src/app/overlay.cpp
	${ROOT}/src/app/roi.cpp
	${ROOT}/src/app/session.cpp
	${ROOT}/src/app/auto_learn.cpp
	${ROOT}/src/app/feature_db.cpp
	${ROOT}/src/app/dataset.cpp
)
target_include_directories(objrec PRIVATE "${ROOT}/include" "${OPENCV_WASM}/include/opencv5")
foreach(lib opencv_geometry opencv_imgproc opencv_flann opencv_core)  # dependents first
	target_link_libraries(objrec PRIVATE "${OPENCV_WASM}/lib/lib${lib}.a")
endforeach()
target_link_libraries(objrec PRIVATE "${OPENCV_WASM}/lib/opencv5/3rdparty/libzlib.a")

# SIMD128 and exceptions must match the OpenCV build (CV_Assert throws).
target_compile_options(objrec PRIVATE -O3 -msimd128 -fexceptions)
target_link_options(objrec PRIVATE
	-O3 -fexceptions
	-sMODULARIZE=1 -sEXPORT_ES6=1 -sEXPORT_NAME=createObjrec
	-sENVIRONMENT=web,worker,node
	-sALLOW_MEMORY_GROWTH=1
	-sEXPORTED_RUNTIME_METHODS=FS,UTF8ToString,stringToNewUTF8,HEAPU8,HEAPF32,HEAPF64
	-sEXPORTED_FUNCTIONS=_malloc,_free
)
set_target_properties(objrec PROPERTIES SUFFIX ".mjs")
