#include "SplatRenderer.h"
#include "Shader.h"
#include "GaussianData.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <sstream>
#include <iostream>

SplatRenderer::SplatRenderer() {}

SplatRenderer::~SplatRenderer() {
    cleanup();
}

uint32_t SplatRenderer::nextPow2(uint32_t v) {
    v--;
    v |= v >> 1; v |= v >> 2; v |= v >> 4;
    v |= v >> 8; v |= v >> 16;
    return v + 1;
}

bool SplatRenderer::compileComputeShader(const std::string& path, GLuint& program) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Failed to open compute shader: " << path << std::endl;
        return false;
    }
    std::stringstream ss;
    ss << file.rdbuf();
    std::string src = ss.str();
    const char* srcPtr = src.c_str();

    GLuint shader = glCreateShader(GL_COMPUTE_SHADER);
    glShaderSource(shader, 1, &srcPtr, nullptr);
    glCompileShader(shader);

    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[1024];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        std::cerr << "Compute shader compile error (" << path << "):\n" << log << std::endl;
        glDeleteShader(shader);
        return false;
    }

    program = glCreateProgram();
    glAttachShader(program, shader);
    glLinkProgram(program);
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char log[1024];
        glGetProgramInfoLog(program, sizeof(log), nullptr, log);
        std::cerr << "Compute program link error (" << path << "):\n" << log << std::endl;
        glDeleteProgram(program);
        program = 0;
        glDeleteShader(shader);
        return false;
    }
    glDeleteShader(shader);
    return true;
}

void SplatRenderer::setupQuadGeometry() {
    float quadVertices[] = {
        -1.0f, -1.0f,
         1.0f, -1.0f,
        -1.0f,  1.0f,
         1.0f,  1.0f,
    };

    glGenBuffers(1, &quadVBO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
}

void SplatRenderer::upload(const GaussianData& data) {
    cleanup();
    if (data.count == 0) return;

    gaussianCount = data.count;
    paddedCount = nextPow2(static_cast<uint32_t>(gaussianCount));

    // Pack Gaussian data into SSBO-friendly layout
    // struct: vec4 pos_opacity, vec4 color_pad, vec4 scale_pad, vec4 rotation = 64 bytes
    std::vector<float> gpuData(gaussianCount * 16);
    for (size_t i = 0; i < gaussianCount; ++i) {
        float* dst = &gpuData[i * 16];
        dst[0]  = data.positions[i].x;
        dst[1]  = data.positions[i].y;
        dst[2]  = data.positions[i].z;
        dst[3]  = data.activatedOpacities[i];
        dst[4]  = data.colors[i].x;
        dst[5]  = data.colors[i].y;
        dst[6]  = data.colors[i].z;
        dst[7]  = 0.0f;  // pad
        dst[8]  = data.activatedScales[i].x;
        dst[9]  = data.activatedScales[i].y;
        dst[10] = data.activatedScales[i].z;
        dst[11] = 0.0f;  // pad

        glm::quat q = (i < data.rotations.size())
            ? glm::normalize(data.rotations[i])
            : glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        dst[12] = q.w;
        dst[13] = q.x;
        dst[14] = q.y;
        dst[15] = q.z;
    }

    // SSBO 0: Gaussian data (static)
    glGenBuffers(1, &gaussianSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, gaussianSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER,
                 static_cast<GLsizeiptr>(gpuData.size() * sizeof(float)),
                 gpuData.data(), GL_STATIC_DRAW);

    // SSBO 1: Sort entries (padded to power of 2)
    // Each entry: {uint key, uint index} = 8 bytes
    glGenBuffers(1, &sortSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, sortSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER,
                 static_cast<GLsizeiptr>(paddedCount * 2 * sizeof(uint32_t)),
                 nullptr, GL_DYNAMIC_DRAW);

    // SSBO 2: Preprocessed 2D splat data
    // Each: vec4 clipCenter + vec4 conicRadius + vec4 colorOpacity + vec4 axisA_axisB = 64 bytes
    glGenBuffers(1, &splat2DSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, splat2DSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER,
                 static_cast<GLsizeiptr>(gaussianCount * 16 * sizeof(float)),
                 nullptr, GL_DYNAMIC_DRAW);

    // Atomic counter buffer
    glGenBuffers(1, &counterBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, counterBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(uint32_t), nullptr, GL_DYNAMIC_DRAW);

    // Radix sort buffers: ping-pong keys and values
    // Keys: uint per element
    GLsizeiptr keyBufSize = static_cast<GLsizeiptr>(gaussianCount * sizeof(uint32_t));
    glGenBuffers(1, &sortKeysA);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, sortKeysA);
    glBufferData(GL_SHADER_STORAGE_BUFFER, keyBufSize, nullptr, GL_DYNAMIC_DRAW);

    glGenBuffers(1, &sortKeysB);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, sortKeysB);
    glBufferData(GL_SHADER_STORAGE_BUFFER, keyBufSize, nullptr, GL_DYNAMIC_DRAW);

    // Values: uint per element (original index)
    glGenBuffers(1, &sortValsA);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, sortValsA);
    glBufferData(GL_SHADER_STORAGE_BUFFER, keyBufSize, nullptr, GL_DYNAMIC_DRAW);

    glGenBuffers(1, &sortValsB);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, sortValsB);
    glBufferData(GL_SHADER_STORAGE_BUFFER, keyBufSize, nullptr, GL_DYNAMIC_DRAW);

    // Histogram buffer: RADIX_SORT_BINS * numWorkgroups * 4 bytes
    const uint32_t ELEMENTS_PER_WG = 256 * 32; // 8192
    radixNumWorkgroups = (static_cast<uint32_t>(gaussianCount) + ELEMENTS_PER_WG - 1) / ELEMENTS_PER_WG;
    GLsizeiptr histBufSize = static_cast<GLsizeiptr>(256 * radixNumWorkgroups * sizeof(uint32_t));
    glGenBuffers(1, &histogramSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, histogramSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER, histBufSize, nullptr, GL_DYNAMIC_DRAW);

    // Compile compute shaders
    compileComputeShader("shaders/preprocess.comp", preprocessProgram);
    compileComputeShader("shaders/sort.comp", bitonicSortProgram);
    compileComputeShader("shaders/radixsort_histograms.comp", histogramProgram);
    compileComputeShader("shaders/radixsort_scatter.comp", scatterProgram);

    // VAO for quad rendering
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);
    setupQuadGeometry();
    glBindVertexArray(0);
}

void SplatRenderer::draw(const Shader& shader, const glm::mat4& modelView,
                         const glm::mat4& projection, const glm::vec2& viewport,
                         float splatScale) {
    if (gaussianCount == 0 || VAO == 0 || !preprocessProgram) return;
    if (!useRadixSort && !bitonicSortProgram) return;
    if (useRadixSort && (!histogramProgram || !scatterProgram)) return;

    auto t0 = std::chrono::high_resolution_clock::now();

    // Bind SSBOs for preprocess
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, gaussianSSBO);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, sortSSBO);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, splat2DSSBO);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, counterBuffer);

    // Bind radix sort key/value buffers for preprocess to write into
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, sortKeysA);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, sortValsA);

    // Reset atomic counter to 0
    uint32_t zero = 0;
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, counterBuffer);
    glClearBufferData(GL_SHADER_STORAGE_BUFFER, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, &zero);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);

    // ---- PASS 1: Preprocess ----
    glUseProgram(preprocessProgram);
    glUniformMatrix4fv(glGetUniformLocation(preprocessProgram, "modelView"), 1, GL_FALSE, &modelView[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(preprocessProgram, "projection"), 1, GL_FALSE, &projection[0][0]);
    glUniform2f(glGetUniformLocation(preprocessProgram, "viewport"), viewport.x, viewport.y);
    glUniform1f(glGetUniformLocation(preprocessProgram, "splatScale"), splatScale);
    glUniform1ui(glGetUniformLocation(preprocessProgram, "gaussianCount"), static_cast<GLuint>(gaussianCount));
    glUniform1ui(glGetUniformLocation(preprocessProgram, "paddedCount"), paddedCount);

    GLuint preprocessGroups = (paddedCount + 255) / 256;
    glDispatchCompute(preprocessGroups, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_ATOMIC_COUNTER_BARRIER_BIT);

    // Read visible count
    uint32_t visibleCount = 0;
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(uint32_t), &visibleCount);
    if (visibleCount > gaussianCount) visibleCount = static_cast<uint32_t>(gaussianCount);
    lastTotalVisible = visibleCount;

    if (visibleCount == 0) {
        lastVisibleCount = 0;
        lastSortTimeMs = 0;
        return;
    }

    if (useRadixSort) {
        // ---- PASS 2: Radix Sort (4 passes x 2 dispatches = 8 dispatches) ----
        uint32_t numElements = static_cast<uint32_t>(gaussianCount);

        // Ping-pong: pass 0,2 read A write B; pass 1,3 read B write A
        GLuint keysRead = sortKeysA, keysWrite = sortKeysB;
        GLuint valsRead = sortValsA, valsWrite = sortValsB;

        for (uint32_t pass = 0; pass < 4; pass++) {
            uint32_t shift = pass * 8;

            // --- Histogram dispatch ---
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, keysRead);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, histogramSSBO);

            // Clear histogram buffer
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, histogramSSBO);
            GLsizeiptr histSize = static_cast<GLsizeiptr>(256 * radixNumWorkgroups * sizeof(uint32_t));
            glClearBufferData(GL_SHADER_STORAGE_BUFFER, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, &zero);
            glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);

            glUseProgram(histogramProgram);
            glUniform1ui(glGetUniformLocation(histogramProgram, "uNumElements"), numElements);
            glUniform1ui(glGetUniformLocation(histogramProgram, "uNumWorkgroups"), radixNumWorkgroups);
            glUniform1ui(glGetUniformLocation(histogramProgram, "uShift"), shift);
            glDispatchCompute(radixNumWorkgroups, 1, 1);
            glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

            // --- Scatter dispatch ---
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, keysRead);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, valsRead);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, keysWrite);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, valsWrite);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, histogramSSBO);

            glUseProgram(scatterProgram);
            glUniform1ui(glGetUniformLocation(scatterProgram, "uNumElements"), numElements);
            glUniform1ui(glGetUniformLocation(scatterProgram, "uNumWorkgroups"), radixNumWorkgroups);
            glUniform1ui(glGetUniformLocation(scatterProgram, "uShift"), shift);
            glDispatchCompute(radixNumWorkgroups, 1, 1);
            glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

            // Swap ping-pong
            std::swap(keysRead, keysWrite);
            std::swap(valsRead, valsWrite);
        }

        // After 4 passes (even number of swaps), result is back in keysRead/valsRead
        // which is sortKeysA/sortValsA (since we swapped 4 times = back to original)
        // The sorted values (indices) are in sortValsA

        auto t1 = std::chrono::high_resolution_clock::now();
        lastSortTimeMs = std::chrono::duration<float, std::milli>(t1 - t0).count();

        // Cap draw count
        // Culled entries (key=0xFFFFFFFF) sort to the end, so visible entries
        // occupy positions 0..(visibleCount-1). Draw the nearest (highest key) ones.
        uint32_t drawCount = std::min(visibleCount, static_cast<uint32_t>(maxVisibleSplats));
        uint32_t startOffset = visibleCount - drawCount;
        lastVisibleCount = drawCount;

        // ---- PASS 3: Render (radix sort path) ----
        // Re-bind splat2D SSBO and the sorted values buffer for the vertex shader
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, sortValsA); // sorted indices
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, splat2DSSBO);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        shader.use();
        shader.setVec2("viewport", viewport);
        glUniform1ui(glGetUniformLocation(shader.ID, "uStartOffset"), startOffset);
        glUniform1ui(glGetUniformLocation(shader.ID, "uUseRadixSort"), 1u);

        glBindVertexArray(VAO);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, static_cast<GLsizei>(drawCount));
        glBindVertexArray(0);

    } else {
        // ---- PASS 2: Bitonic Sort (fallback) ----
        // Re-bind sortSSBO at binding 1 for bitonic sort
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, sortSSBO);

        glUseProgram(bitonicSortProgram);
        GLint jLoc = glGetUniformLocation(bitonicSortProgram, "uJ");
        GLint kLoc = glGetUniformLocation(bitonicSortProgram, "uK");
        GLuint sortGroups = (paddedCount + 255) / 256;

        for (uint32_t k = 2; k <= paddedCount; k <<= 1) {
            for (uint32_t j = k >> 1; j >= 1; j >>= 1) {
                glUniform1ui(jLoc, j);
                glUniform1ui(kLoc, k);
                glDispatchCompute(sortGroups, 1, 1);
                glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
            }
        }

        auto t1 = std::chrono::high_resolution_clock::now();
        lastSortTimeMs = std::chrono::duration<float, std::milli>(t1 - t0).count();

        // Cap draw count — draw the NEAREST splats
        uint32_t drawCount = std::min(visibleCount, static_cast<uint32_t>(maxVisibleSplats));
        uint32_t startOffset = visibleCount - drawCount;
        lastVisibleCount = drawCount;

        // ---- PASS 3: Render (bitonic path) ----
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        shader.use();
        shader.setVec2("viewport", viewport);
        glUniform1ui(glGetUniformLocation(shader.ID, "uStartOffset"), startOffset);
        glUniform1ui(glGetUniformLocation(shader.ID, "uUseRadixSort"), 0u);

        glBindVertexArray(VAO);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, static_cast<GLsizei>(drawCount));
        glBindVertexArray(0);
    }
}

void SplatRenderer::cleanup() {
    if (VAO) { glDeleteVertexArrays(1, &VAO); VAO = 0; }
    if (quadVBO) { glDeleteBuffers(1, &quadVBO); quadVBO = 0; }
    if (gaussianSSBO) { glDeleteBuffers(1, &gaussianSSBO); gaussianSSBO = 0; }
    if (sortSSBO) { glDeleteBuffers(1, &sortSSBO); sortSSBO = 0; }
    if (splat2DSSBO) { glDeleteBuffers(1, &splat2DSSBO); splat2DSSBO = 0; }
    if (counterBuffer) { glDeleteBuffers(1, &counterBuffer); counterBuffer = 0; }
    if (sortKeysA) { glDeleteBuffers(1, &sortKeysA); sortKeysA = 0; }
    if (sortKeysB) { glDeleteBuffers(1, &sortKeysB); sortKeysB = 0; }
    if (sortValsA) { glDeleteBuffers(1, &sortValsA); sortValsA = 0; }
    if (sortValsB) { glDeleteBuffers(1, &sortValsB); sortValsB = 0; }
    if (histogramSSBO) { glDeleteBuffers(1, &histogramSSBO); histogramSSBO = 0; }
    if (preprocessProgram) { glDeleteProgram(preprocessProgram); preprocessProgram = 0; }
    if (bitonicSortProgram) { glDeleteProgram(bitonicSortProgram); bitonicSortProgram = 0; }
    if (histogramProgram) { glDeleteProgram(histogramProgram); histogramProgram = 0; }
    if (scatterProgram) { glDeleteProgram(scatterProgram); scatterProgram = 0; }
    gaussianCount = 0;
    paddedCount = 0;
    radixNumWorkgroups = 0;
}
