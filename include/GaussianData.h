#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <vector>
#include <array>
#include <string>
#include <algorithm>
#include <cmath>

// SH coefficient C0 = 0.28209479177387814
static constexpr float SH_C0 = 0.28209479177387814f;

struct GaussianData {
    // Per-Gaussian raw properties (SoA layout)
    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec3> shDC;                    // SH degree-0 (3 coefficients)
    std::vector<std::array<float, 45>> shRest;      // SH rest (up to degree 3)
    std::vector<float> opacities;                    // Raw opacity (pre-sigmoid)
    std::vector<glm::vec3> scales;                   // Raw scale (pre-exp)
    std::vector<glm::quat> rotations;

    // Derived data (computed from raw)
    std::vector<glm::vec3> colors;                   // RGB from SH DC
    std::vector<float> activatedOpacities;           // sigmoid(opacity)
    std::vector<glm::vec3> activatedScales;          // exp(scale)

    // Metadata
    size_t count = 0;
    std::string sourcePath;
    glm::vec3 bboxMin{0.0f};
    glm::vec3 bboxMax{0.0f};
    glm::vec3 centroid{0.0f};

    void computeDerivedData() {
        colors.resize(count);
        activatedOpacities.resize(count);
        activatedScales.resize(count);

        glm::vec3 sum{0.0f};
        bboxMin = glm::vec3(std::numeric_limits<float>::max());
        bboxMax = glm::vec3(std::numeric_limits<float>::lowest());

        for (size_t i = 0; i < count; ++i) {
            // SH DC to RGB: color = clamp(0.5 + SH_C0 * dc, 0, 1)
            if (i < shDC.size()) {
                colors[i] = glm::clamp(
                    glm::vec3(0.5f) + SH_C0 * shDC[i],
                    glm::vec3(0.0f), glm::vec3(1.0f)
                );
            } else {
                colors[i] = glm::vec3(0.5f);
            }

            // Sigmoid activation for opacity
            if (i < opacities.size()) {
                activatedOpacities[i] = 1.0f / (1.0f + std::exp(-opacities[i]));
            } else {
                activatedOpacities[i] = 1.0f;
            }

            // Exp activation for scale
            if (i < scales.size()) {
                activatedScales[i] = glm::vec3(
                    std::exp(scales[i].x),
                    std::exp(scales[i].y),
                    std::exp(scales[i].z)
                );
            } else {
                activatedScales[i] = glm::vec3(1.0f);
            }

            // Bounding box and centroid accumulation
            const glm::vec3& p = positions[i];
            bboxMin = glm::min(bboxMin, p);
            bboxMax = glm::max(bboxMax, p);
            sum += p;
        }

        if (count > 0) {
            centroid = sum / static_cast<float>(count);
        }
    }

    void clear() {
        positions.clear();
        normals.clear();
        shDC.clear();
        shRest.clear();
        opacities.clear();
        scales.clear();
        rotations.clear();
        colors.clear();
        activatedOpacities.clear();
        activatedScales.clear();
        count = 0;
        sourcePath.clear();
        bboxMin = glm::vec3(0.0f);
        bboxMax = glm::vec3(0.0f);
        centroid = glm::vec3(0.0f);
    }
};
