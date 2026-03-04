#pragma once

#include <glad/gl.h>

class Shader;

class Renderer {
public:
    Renderer();
    ~Renderer();

    void init();
    void draw(const Shader& shader);
    void cleanup();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

private:
    unsigned int VAO = 0;
    unsigned int VBO = 0;

    void setupTriangle();
};
