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

    // Model matrix — set by caller before draw() so normals transform correctly
    glm::mat4 modelMatrix = glm::mat4(1.0f);

    // Lighting parameters (controlled via ImGui)
    bool  lightingEnabled = true;
    float lightAzimuth    = 45.0f;   // degrees
    float lightElevation  = 45.0f;   // degrees
    float lightColor[3]   = {1.0f, 1.0f, 0.95f};
    float lightIntensity  = 1.0f;
    float ambient         = 0.3f;
    float diffuse         = 0.7f;
    float wrapFactor      = 0.3f;    // wrap lighting
    float scatter         = 0.2f;    // subsurface scatter

private:
    void setupQuadGeometry();
    void setLightUniforms(const Shader& shader);
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

    // Async visible count readback (eliminates CPU-GPU sync stall)
    GLuint readbackBuf[2] = {0, 0};
    GLsync readbackFence[2] = {nullptr, nullptr};
    uint32_t* readbackPtr[2] = {nullptr, nullptr};
    int readbackIdx = 0;
    uint32_t asyncVisibleCount = 0;
    bool asyncReady = false;  // false until first readback completes

    size_t lastVisibleCount = 0;
    size_t lastTotalVisible = 0;
    float lastSortTimeMs = 0.0f;
};
