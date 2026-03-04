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
    // Each: vec4 clipCenter + vec4 conicRadius + vec4 colorOpacity = 48 bytes
    glGenBuffers(1, &splat2DSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, splat2DSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER,
                 static_cast<GLsizeiptr>(gaussianCount * 12 * sizeof(float)),
                 nullptr, GL_DYNAMIC_DRAW);

    // Atomic counter buffer
    glGenBuffers(1, &counterBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, counterBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(uint32_t), nullptr, GL_DYNAMIC_DRAW);

    // Compile compute shaders
    compileComputeShader("shaders/preprocess.comp", preprocessProgram);
    compileComputeShader("shaders/sort.comp", sortProgram);

    // VAO for quad rendering
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);
    setupQuadGeometry();
    glBindVertexArray(0);
}

void SplatRenderer::draw(const Shader& shader, const glm::mat4& modelView,
                         const glm::mat4& projection, const glm::vec2& viewport,
                         float splatScale) {
    if (gaussianCount == 0 || VAO == 0 || !preprocessProgram || !sortProgram) return;

    auto t0 = std::chrono::high_resolution_clock::now();

    // Bind SSBOs
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, gaussianSSBO);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, sortSSBO);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, splat2DSSBO);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, counterBuffer);

    // Reset atomic counter to 0 via compute-friendly clear
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

    // Read visible count (implicit GPU sync via glGetBufferSubData)
    uint32_t visibleCount = 0;
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(uint32_t), &visibleCount);

    // Clamp to sane range in case of any GPU issue
    if (visibleCount > gaussianCount) visibleCount = static_cast<uint32_t>(gaussianCount);
    lastTotalVisible = visibleCount;

    if (visibleCount == 0) {
        lastVisibleCount = 0;
        lastSortTimeMs = 0;
        return;
    }

    // ---- PASS 2: Bitonic Sort ----
    glUseProgram(sortProgram);
    GLint jLoc = glGetUniformLocation(sortProgram, "uJ");
    GLint kLoc = glGetUniformLocation(sortProgram, "uK");
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

    // Cap draw count — draw the NEAREST splats (at the end of back-to-front sorted array)
    uint32_t drawCount = std::min(visibleCount, static_cast<uint32_t>(maxVisibleSplats));
    uint32_t startOffset = visibleCount - drawCount;  // skip farthest, keep nearest
    lastVisibleCount = drawCount;

    // ---- PASS 3: Render ----
    // Full barrier to ensure all compute writes visible to vertex/fragment shader
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    shader.use();
    shader.setVec2("viewport", viewport);
    glUniform1ui(glGetUniformLocation(shader.ID, "uStartOffset"), startOffset);

    glBindVertexArray(VAO);
    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, static_cast<GLsizei>(drawCount));
    glBindVertexArray(0);
}

void SplatRenderer::cleanup() {
    if (VAO) { glDeleteVertexArrays(1, &VAO); VAO = 0; }
    if (quadVBO) { glDeleteBuffers(1, &quadVBO); quadVBO = 0; }
    if (gaussianSSBO) { glDeleteBuffers(1, &gaussianSSBO); gaussianSSBO = 0; }
    if (sortSSBO) { glDeleteBuffers(1, &sortSSBO); sortSSBO = 0; }
    if (splat2DSSBO) { glDeleteBuffers(1, &splat2DSSBO); splat2DSSBO = 0; }
    if (counterBuffer) { glDeleteBuffers(1, &counterBuffer); counterBuffer = 0; }
    if (preprocessProgram) { glDeleteProgram(preprocessProgram); preprocessProgram = 0; }
    if (sortProgram) { glDeleteProgram(sortProgram); sortProgram = 0; }
    gaussianCount = 0;
    paddedCount = 0;
}
