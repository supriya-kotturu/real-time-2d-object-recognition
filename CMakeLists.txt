cmake_minimum_required(VERSION 3.20)
project(objrec LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# The prebuilt OpenCV at C:/opencv is MSVC-only; use the "msvc" preset so MinGW is never picked.
if((NOT OpenCV_DIR OR OpenCV_DIR MATCHES "-NOTFOUND$")
		AND EXISTS "C:/opencv/build/OpenCVConfig.cmake")
	set(OpenCV_DIR "C:/opencv/build" CACHE PATH "Directory containing OpenCVConfig.cmake" FORCE)
endif()
find_package(OpenCV REQUIRED)

# Executables land in out/ for every config so data paths stay relative to the repo root.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/out")
foreach(config ${CMAKE_CONFIGURATION_TYPES})
	string(TOUPPER "${config}" config_upper)
	set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${config_upper} "${PROJECT_SOURCE_DIR}/out")
endforeach()

if(MSVC)
	add_compile_options(/W4 /utf-8)
else()
	add_compile_options(-Wall -Wextra)
endif()

# --- objrec_core: pipeline stages (threshold, morphology, regions, features, classify) ---
# Pure cv::Mat in/out. Must never link highgui/videoio: keeps the WASM path (D-03) open.
add_library(objrec_core STATIC
	src/core/threshold.cpp
	src/core/morphology.cpp
	src/core/regions.cpp
	src/core/features.cpp
	src/core/classifier.cpp
	src/core/evaluation.cpp
)
target_include_directories(objrec_core PUBLIC "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(objrec_core PUBLIC opencv_core opencv_imgproc)

# --- objrec_app: glue shared by executables (input sources, windows, CSV) ---
add_library(objrec_app STATIC
	src/app/frame_source.cpp
	src/app/pipeline.cpp
	src/app/overlay.cpp
	src/app/feature_db.cpp
	src/app/capture.cpp
	src/app/dataset.cpp
)
target_include_directories(objrec_app PUBLIC "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(objrec_app PUBLIC objrec_core ${OpenCV_LIBS})

if(WIN32)
	file(GLOB OPENCV_RUNTIME_DLLS "${OpenCV_DIR}/x64/*/bin/*.dll" "${OpenCV_DIR}/bin/*.dll")
endif()

function(add_objrec_executable target)
	add_executable(${target} ${ARGN})
	target_link_libraries(${target} PRIVATE objrec_core objrec_app)
	set_target_properties(${target} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")
	if(WIN32 AND OPENCV_RUNTIME_DLLS)
		add_custom_command(TARGET ${target} POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy_if_different ${OPENCV_RUNTIME_DLLS} "$<TARGET_FILE_DIR:${target}>"
			COMMAND_EXPAND_LISTS
		)
	endif()
endfunction()

# D-04 option B: one lean executable per job.
add_objrec_executable(live src/apps/live.cpp)
add_objrec_executable(inspect src/apps/inspect.cpp)
add_objrec_executable(train src/apps/train.cpp)
add_objrec_executable(eval src/apps/eval.cpp)
add_objrec_executable(tests tests/morphology_tests.cpp)  # D-05

# Web GUI backend (D-13). cpp-httplib v0.60.1 is vendored header-only; SYSTEM keeps its
# warnings out of our /W4 output.
add_objrec_executable(server src/apps/server.cpp)
target_include_directories(server SYSTEM PRIVATE "${PROJECT_SOURCE_DIR}/third_party/cpp-httplib")
