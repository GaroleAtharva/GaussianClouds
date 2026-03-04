#pragma once

#include <glad/gl.h>

class Shader;
struct GaussianData;

class PointCloudRenderer {
public:
    PointCloudRenderer();
    ~PointCloudRenderer();

    void upload(const GaussianData& data);
    void draw(const Shader& shader);
    void cleanup();

    PointCloudRenderer(const PointCloudRenderer&) = delete;
    PointCloudRenderer& operator=(const PointCloudRenderer&) = delete;

    size_t getVertexCount() const { return vertexCount; }

private:
    unsigned int VAO = 0;
    unsigned int VBO = 0;
    size_t vertexCount = 0;
};
