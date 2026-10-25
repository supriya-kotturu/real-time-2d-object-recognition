#pragma once

#include <opencv2/core.hpp>
#include <vector>

#include "core/features.h"

// T9 steps A-D without the network (D-27): kept apart from embedding.h so the browser build,
// which has no OpenCV dnn module, prepares exactly the same ResNet18 input as server.exe.
// Approach follows Bruce A. Maxwell's course utilities (third_party/maxwell/utilities.cpp).

// Steps A-C: rotate the ORIGINAL frame about the centroid so the primary axis points along +x,
// then crop the oriented bounding box. The axis has a 180 deg ambiguity; it is resolved so the
// region's third moment along the axis is positive, so the same object rotated by 180 deg
// yields the same ROI. Returns false if nothing of the box lies inside the rotated frame.
bool extractAlignedRoi(const cv::Mat& frame, const cv::Mat& regionMap, int regionId,
                       const objectrecognition::RegionFeatures& f, cv::Mat& roi);

constexpr int kResnetSide = 224;

// Step D: BGR ROI -> 1x3x224x224 float tensor (NCHW, RGB), ImageNet-normalised as in the
// course code. Matches cv::dnn::blobFromImage(roi, 1/(255*0.226), 224x224, (124,116,104),
// swapRB=true, crop=false): resize without cropping, so elongated objects are squashed, the
// same way for DB and query images.
void makeResnetInput(const cv::Mat& roiBgr, std::vector<float>& nchw);
