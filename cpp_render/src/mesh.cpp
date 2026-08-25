#include "mesh.h"
#include <cstddef>

Mesh::Mesh()
    : m_VAO(0), m_VBO(0), m_EBO(0), m_materialIndex(0), m_nodeMatrix(1.0f) {}

Mesh::Mesh(std::vector<Vertex> vertices, std::vector<unsigned int> indices, int materialIndex, const glm::mat4& nodeTransform)
    : m_VAO(0), m_VBO(0), m_EBO(0),
      m_vertices(std::move(vertices)),
      m_indices(std::move(indices)),
      m_materialIndex(materialIndex),
      m_nodeMatrix(nodeTransform) {
    setupMesh();
}

Mesh::~Mesh() {
    cleanup();
}

Mesh::Mesh(Mesh&& other) noexcept
    : m_VAO(other.m_VAO), m_VBO(other.m_VBO), m_EBO(other.m_EBO),
      m_vertices(std::move(other.m_vertices)),
      m_indices(std::move(other.m_indices)),
      m_materialIndex(other.m_materialIndex),
      m_nodeMatrix(other.m_nodeMatrix) {
    other.m_VAO = 0;
    other.m_VBO = 0;
    other.m_EBO = 0;
}

Mesh& Mesh::operator=(Mesh&& other) noexcept {
    if (this != &other) {
        cleanup();
        m_VAO = other.m_VAO;
        m_VBO = other.m_VBO;
        m_EBO = other.m_EBO;
        m_vertices = std::move(other.m_vertices);
        m_indices = std::move(other.m_indices);
        m_materialIndex = other.m_materialIndex;
        m_nodeMatrix = other.m_nodeMatrix;

        other.m_VAO = 0;
        other.m_VBO = 0;
        other.m_EBO = 0;
    }
    return *this;
}

void Mesh::cleanup() {
    if (m_VAO != 0) {
        glDeleteVertexArrays(1, &m_VAO);
        m_VAO = 0;
    }
    if (m_VBO != 0) {
        glDeleteBuffers(1, &m_VBO);
        m_VBO = 0;
    }
    if (m_EBO != 0) {
        glDeleteBuffers(1, &m_EBO);
        m_EBO = 0;
    }
}

void Mesh::setupMesh() {
    if (m_vertices.empty()) return;

    cleanup();

    glGenVertexArrays(1, &m_VAO);
    glGenBuffers(1, &m_VBO);

    glBindVertexArray(m_VAO);

    // Upload vertices
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferData(GL_ARRAY_BUFFER, m_vertices.size() * sizeof(Vertex), m_vertices.data(), GL_STATIC_DRAW);

    // Upload indices if present
    if (!m_indices.empty()) {
        glGenBuffers(1, &m_EBO);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_indices.size() * sizeof(unsigned int), m_indices.data(), GL_STATIC_DRAW);
    }

    // Attribute 0: Position
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position));

    // Attribute 1: Normal
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));

    // Attribute 2: TexCoords
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texcoord));

    // Attribute 3: Tangent
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, tangent));

    glBindVertexArray(0);
}

void Mesh::draw() const {
    if (m_VAO == 0) return;

    glBindVertexArray(m_VAO);
    if (!m_indices.empty()) {
        glDrawElements(GL_TRIANGLES, (GLsizei)m_indices.size(), GL_UNSIGNED_INT, 0);
    } else {
        glDrawArrays(GL_TRIANGLES, 0, (GLsizei)m_vertices.size());
    }
    glBindVertexArray(0);
}
