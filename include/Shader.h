#pragma once

#include <glad/gl.h>
#include <glm/glm.hpp>

#include <string>

class Shader {
public:
    unsigned int ID;

    Shader(const std::string& vertexPath, const std::string& fragmentPath);
    ~Shader();

    void use() const;

    void setBool(const std::string& name, bool value) const;
    void setInt(const std::string& name, int value) const;
    void setFloat(const std::string& name, float value) const;
    void setVec2(const std::string& name, const glm::vec2& value) const;
    void setVec3(const std::string& name, const glm::vec3& value) const;
    void setVec4(const std::string& name, const glm::vec4& value) const;
    void setMat4(const std::string& name, const glm::mat4& value) const;

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

private:
    void checkCompileErrors(unsigned int shader, const std::string& type);
};
