#pragma once

#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <vector>

#include "material.h"
#include "mesh.h"
#include "texture.h"

// Forward declaration
class ShaderManager;

/**
 * @class Model
 * @brief Represents a complete 3D scene entity consisting of multiple submeshes,
 *        PBR materials, textures, and bounding box normalization.
 */
class Model {
public:
    Model();
    ~Model();

    // Move semantics
    Model(Model&& other) noexcept;
    Model& operator=(Model&& other) noexcept;

    // Delete copy to prevent duplicate GPU resource ownership
    Model(const Model&) = delete;
    Model& operator=(const Model&) = delete;

    /** Add a submesh primitive to the model. */
    void addMesh(Mesh mesh);

    /** Add a material definition to the model. */
    void addMaterial(const Material& material);

    /** Add an allocated texture to the model. */
    void addTexture(std::unique_ptr<Texture> texture);

    /** Computes auto-centering and uniform scaling matrix based on mesh vertices. */
    void computeBoundsAndNormalization();

    /**
     * @brief Render all submeshes, binding per-mesh materials and textures to the active shader.
     * @param activeShader Shader currently in use for uniforms and texture bindings.
     * @param rootTransform World-space transform to apply.
     */
    void draw(ShaderManager* activeShader, const glm::mat4& rootTransform) const;

    /** Clear all meshes, materials, and textures. */
    void clear();

    bool isEmpty() const { return m_meshes.empty(); }

    // --- Statistics & Accessors ---
    size_t getMeshCount() const { return m_meshes.size(); }
    size_t getVertexCount() const { return m_totalVertices; }
    size_t getTriangleCount() const { return m_totalTriangles; }
    size_t getTextureCount() const { return m_textures.size(); }
    size_t getMaterialCount() const { return m_materials.size(); }

    const glm::mat4& getNormalizationMatrix() const { return m_normalizationMatrix; }
    const std::vector<Material>& getMaterials() const { return m_materials; }
    const std::vector<Mesh>& getMeshes() const { return m_meshes; }

private:
    std::vector<Mesh>                     m_meshes;
    std::vector<Material>                 m_materials;
    std::vector<std::unique_ptr<Texture>> m_textures;

    glm::mat4 m_normalizationMatrix;
    size_t    m_totalVertices;
    size_t    m_totalTriangles;
};
