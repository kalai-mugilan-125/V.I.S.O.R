#pragma once

#include <glad/glad.h>
#include <string>
#include <glm/glm.hpp>

class ShaderManager {
public:
    ShaderManager();
    ~ShaderManager();

    bool compileShaders(const std::string& vertexSrcPath, const std::string& fragmentSrcPath);
    bool compileFromSource(const std::string& vertexSrc, const std::string& fragmentSrc);
    void use();
    void applyHolographicFade(float depth);

    // Uniform setters
    void setBool(const std::string& name, bool value) const;
    void setInt(const std::string& name, int value) const;
    void setFloat(const std::string& name, float value) const;
    void setVec3(const std::string& name, const glm::vec3& value) const;
    void setMat4(const std::string& name, const glm::mat4& mat) const;

    GLuint getProgramID() const { return shaderProgram; }

private:
    GLuint shaderProgram;

    void checkCompileErrors(GLuint shader, std::string type);
};
