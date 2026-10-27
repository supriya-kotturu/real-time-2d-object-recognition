#pragma once

#include <string>
#include <vector>

#include "core/classifier.h"

// D-12: headerless CSV, one row per sample: label,f1,...,fk

// Missing file -> empty DB (first training run).
std::vector<objectrecognition::Sample> loadFeatureDb(const std::string& path);

// Rewrites the whole file; used when DB entries are deleted from the GUI.
void saveFeatureDb(const std::string& path, const std::vector<objectrecognition::Sample>& db);

void appendSample(const std::string& path, const objectrecognition::Sample& sample);
