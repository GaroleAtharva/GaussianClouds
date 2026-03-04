#pragma once

#include <string>

struct GaussianData;

struct PLYLoadResult {
    bool success = false;
    std::string error;
    double loadTimeMs = 0.0;
    size_t fileSizeBytes = 0;
    size_t vertexCount = 0;
};

class PLYLoader {
public:
    static PLYLoadResult load(const std::string& filepath, GaussianData& outData);
};
