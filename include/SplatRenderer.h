#pragma once

#include <glad/gl.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <vector>
#include <cstdint>
#include <string>

class Shader;
struct GaussianData;

class SplatRenderer {
public:
    SplatRenderer();
    ~SplatRenderer();

    void upload(const GaussianData& data);
    void draw(const Shader& shader, const glm::mat4& modelView,
              const glm::mat4& projection, const glm::vec2& viewport,
              float splatScale);
    void cleanup();

    SplatRenderer(const SplatRenderer&) = delete;
    SplatRenderer& operator=(const SplatRenderer&) = delete;

    size_t getInstanceCount() const { return gaussianCount; }
    size_t getVisibleCount() const { return lastVisibleCount; }
    size_t getTotalVisible() const { return lastTotalVisible; }
    float getLastSortTimeMs() const { return lastSortTimeMs; }

    int maxVisibleSplats = 200000;  // configurable via ImGui
    bool useRadixSort = true;       // toggle between radix and bitonic sort

private:
    void setupQuadGeometry();
    bool compileComputeShader(const std::string& path, GLuint& program);
    static uint32_t nextPow2(uint32_t v);

    // Rendering
    GLuint VAO = 0;
    GLuint quadVBO = 0;

    // Compute shader programs
    GLuint preprocessProgram = 0;
    GLuint bitonicSortProgram = 0;  // bitonic sort (fallback)
    GLuint histogramProgram = 0;    // radix sort: histogram pass
    GLuint scatterProgram = 0;      // radix sort: scatter pass

    // SSBOs
    GLuint gaussianSSBO = 0;   // binding 0: raw Gaussian data (static)
    GLuint sortSSBO = 0;       // binding 1: sort entries {key, index} (bitonic)
    GLuint splat2DSSBO = 0;    // binding 2: preprocessed 2D splat data
    GLuint counterBuffer = 0;  // binding 3: atomic visible count

    // Radix sort ping-pong buffers
    GLuint sortKeysA = 0, sortKeysB = 0;   // key ping-pong
    GLuint sortValsA = 0, sortValsB = 0;   // value ping-pong
    GLuint histogramSSBO = 0;              // per-workgroup histograms

    size_t gaussianCount = 0;
    uint32_t paddedCount = 0;
    uint32_t radixNumWorkgroups = 0;

    size_t lastVisibleCount = 0;
    size_t lastTotalVisible = 0;
    float lastSortTimeMs = 0.0f;
};
