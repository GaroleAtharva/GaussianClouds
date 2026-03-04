#include "PointCloudRenderer.h"
#include "Shader.h"
#include "GaussianData.h"

#include <vector>

PointCloudRenderer::PointCloudRenderer() {}

PointCloudRenderer::~PointCloudRenderer() {
    cleanup();
}

void PointCloudRenderer::upload(const GaussianData& data) {
    cleanup();

    if (data.count == 0) return;

    vertexCount = data.count;

    // Build interleaved buffer: [pos.xyz, col.rgb] per vertex (same layout as triangle renderer)
    std::vector<float> buffer(vertexCount * 6);
    for (size_t i = 0; i < vertexCount; ++i) {
        size_t offset = i * 6;
        buffer[offset + 0] = data.positions[i].x;
        buffer[offset + 1] = data.positions[i].y;
        buffer[offset + 2] = data.positions[i].z;
        buffer[offset + 3] = data.colors[i].x;
        buffer[offset + 4] = data.colors[i].y;
        buffer[offset + 5] = data.colors[i].z;
    }

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(buffer.size() * sizeof(float)),
                 buffer.data(), GL_STATIC_DRAW);

    // Position attribute (location = 0)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Color attribute (location = 1)
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

void PointCloudRenderer::draw(const Shader& shader) {
    if (vertexCount == 0 || VAO == 0) return;

    shader.use();
    glBindVertexArray(VAO);
    glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(vertexCount));
    glBindVertexArray(0);
}

void PointCloudRenderer::cleanup() {
    if (VAO) {
        glDeleteVertexArrays(1, &VAO);
        VAO = 0;
    }
    if (VBO) {
        glDeleteBuffers(1, &VBO);
        VBO = 0;
    }
    vertexCount = 0;
}
