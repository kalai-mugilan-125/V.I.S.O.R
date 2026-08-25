#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <vector>

/**
 * @struct Vertex
 * @brief Interleaved vertex layout (matches layout locations 0, 1, 2, 3 in vertex shaders).
 */
struct Vertex {
    glm::vec3 position;  ///< Location 0
    glm::vec3 normal;    ///< Location 1
    glm::vec2 texcoord;  ///< Location 2
    glm::vec4 tangent;   ///< Location 3
};

/**
 * @class Mesh
 * @brief Encapsulates a single GPU drawable geometry primitive with its own VAO/VBO/EBO,
 *        vertex/index buffers, RAII memory management, and material association.
 */
class Mesh {
public:
    Mesh();
    Mesh(std::vector<Vertex> vertices, std::vector<unsigned int> indices, int materialIndex = 0, const glm::mat4& nodeTransform = glm::mat4(1.0f));
    ~Mesh();

    // Disable copy to prevent double GPU resource deletion
    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;

    // Move semantics
    Mesh(Mesh&& other) noexcept;
    Mesh& operator=(Mesh&& other) noexcept;

    /**
     * @brief Setup and upload vertex/index buffers to OpenGL GPU memory.
     */
    void setupMesh();

    /**
     * @brief Issue the draw call for this mesh primitive.
     */
    void draw() const;

    /**
     * @brief Release OpenGL VAO/VBO/EBO resources.
     */
    void cleanup();

    // Getters & Setters
    int getMaterialIndex() const { return m_materialIndex; }
    void setMaterialIndex(int idx) { m_materialIndex = idx; }

    const glm::mat4& getNodeMatrix() const { return m_nodeMatrix; }
    void setNodeMatrix(const glm::mat4& mat) { m_nodeMatrix = mat; }

    size_t getVertexCount() const { return m_vertices.size(); }
    size_t getIndexCount() const { return m_indices.size(); }
    size_t getTriangleCount() const { return m_indices.empty() ? (m_vertices.size() / 3) : (m_indices.size() / 3); }

    const std::vector<Vertex>& getVertices() const { return m_vertices; }
    const std::vector<unsigned int>& getIndices() const { return m_indices; }

private:
    GLuint m_VAO;
    GLuint m_VBO;
    GLuint m_EBO;

    std::vector<Vertex>       m_vertices;
    std::vector<unsigned int> m_indices;
    int                       m_materialIndex;
    glm::mat4                 m_nodeMatrix;
};
