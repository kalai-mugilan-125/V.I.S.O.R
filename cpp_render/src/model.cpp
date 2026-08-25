#include "model.h"
#include "shader_manager.h"
#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>

Model::Model()
    : m_normalizationMatrix(1.0f), m_totalVertices(0), m_totalTriangles(0) {}

Model::~Model() {
    clear();
}

Model::Model(Model&& other) noexcept
    : m_meshes(std::move(other.m_meshes)),
      m_materials(std::move(other.m_materials)),
      m_textures(std::move(other.m_textures)),
      m_normalizationMatrix(other.m_normalizationMatrix),
      m_totalVertices(other.m_totalVertices),
      m_totalTriangles(other.m_totalTriangles) {
    other.m_totalVertices = 0;
    other.m_totalTriangles = 0;
}

Model& Model::operator=(Model&& other) noexcept {
    if (this != &other) {
        clear();
        m_meshes = std::move(other.m_meshes);
        m_materials = std::move(other.m_materials);
        m_textures = std::move(other.m_textures);
        m_normalizationMatrix = other.m_normalizationMatrix;
        m_totalVertices = other.m_totalVertices;
        m_totalTriangles = other.m_totalTriangles;

        other.m_totalVertices = 0;
        other.m_totalTriangles = 0;
    }
    return *this;
}

void Model::clear() {
    m_meshes.clear();
    m_materials.clear();
    m_textures.clear();
    m_normalizationMatrix = glm::mat4(1.0f);
    m_totalVertices = 0;
    m_totalTriangles = 0;
}

void Model::addMesh(Mesh mesh) {
    m_totalVertices += mesh.getVertexCount();
    m_totalTriangles += mesh.getTriangleCount();
    m_meshes.push_back(std::move(mesh));
}

void Model::addMaterial(const Material& material) {
    m_materials.push_back(material);
}

void Model::addTexture(std::unique_ptr<Texture> texture) {
    m_textures.push_back(std::move(texture));
}

void Model::computeBoundsAndNormalization() {
    if (m_meshes.empty()) return;

    glm::vec3 minBound(1e9f);
    glm::vec3 maxBound(-1e9f);

    for (const auto& mesh : m_meshes) {
        const auto& verts = mesh.getVertices();
        const auto& nodeMat = mesh.getNodeMatrix();
        for (const auto& v : verts) {
            glm::vec3 worldPos = glm::vec3(nodeMat * glm::vec4(v.position, 1.0f));
            minBound = glm::min(minBound, worldPos);
            maxBound = glm::max(maxBound, worldPos);
        }
    }

    if (minBound.x < maxBound.x && minBound.y < maxBound.y && minBound.z < maxBound.z) {
        glm::vec3 center = (minBound + maxBound) * 0.5f;
        glm::vec3 size = maxBound - minBound;
        float maxDim = std::max(size.x, std::max(size.y, size.z));
        if (maxDim > 0.0001f) {
            float scale = 3.0f / maxDim;
            m_normalizationMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(scale)) *
                                    glm::translate(glm::mat4(1.0f), -center);
        }
    }
}

void Model::draw(ShaderManager* activeShader, const glm::mat4& rootTransform) const {
    if (m_meshes.empty()) return;

    for (const auto& mesh : m_meshes) {
        if (activeShader) {
            glm::mat4 fullModel = rootTransform * mesh.getNodeMatrix();
            activeShader->setMat4("model", fullModel);

            int matIdx = mesh.getMaterialIndex();
            if (matIdx >= 0 && matIdx < (int)m_materials.size()) {
                const auto& mat = m_materials[matIdx];

                activeShader->setVec3("u_BaseColorFactor", glm::vec3(mat.baseColorFactor));
                activeShader->setFloat("u_AlphaFactor", mat.baseColorFactor.a);
                activeShader->setFloat("u_MetallicFactor", mat.metallicFactor);
                activeShader->setFloat("u_RoughnessFactor", mat.roughnessFactor);
                activeShader->setVec3("u_EmissiveFactor", mat.emissiveFactor);

                // Base Color Texture (Unit 0)
                if (mat.baseColorTextureIndex >= 0 && mat.baseColorTextureIndex < (int)m_textures.size() && m_textures[mat.baseColorTextureIndex]) {
                    m_textures[mat.baseColorTextureIndex]->bind(0);
                    activeShader->setInt("u_BaseColorSampler", 0);
                    activeShader->setBool("u_HasBaseColorTex", true);
                } else {
                    activeShader->setBool("u_HasBaseColorTex", false);
                }

                // Normal Texture (Unit 1)
                if (mat.normalTextureIndex >= 0 && mat.normalTextureIndex < (int)m_textures.size() && m_textures[mat.normalTextureIndex]) {
                    m_textures[mat.normalTextureIndex]->bind(1);
                    activeShader->setInt("u_NormalSampler", 1);
                    activeShader->setBool("u_HasNormalTex", true);
                } else {
                    activeShader->setBool("u_HasNormalTex", false);
                }

                // Metallic Roughness Texture (Unit 2)
                if (mat.metallicRoughnessTextureIndex >= 0 && mat.metallicRoughnessTextureIndex < (int)m_textures.size() && m_textures[mat.metallicRoughnessTextureIndex]) {
                    m_textures[mat.metallicRoughnessTextureIndex]->bind(2);
                    activeShader->setInt("u_MetallicRoughnessSampler", 2);
                    activeShader->setBool("u_HasMetallicRoughnessTex", true);
                } else {
                    activeShader->setBool("u_HasMetallicRoughnessTex", false);
                }

                // Emissive Texture (Unit 3)
                if (mat.emissiveTextureIndex >= 0 && mat.emissiveTextureIndex < (int)m_textures.size() && m_textures[mat.emissiveTextureIndex]) {
                    m_textures[mat.emissiveTextureIndex]->bind(3);
                    activeShader->setInt("u_EmissiveSampler", 3);
                    activeShader->setBool("u_HasEmissiveTex", true);
                } else {
                    activeShader->setBool("u_HasEmissiveTex", false);
                }

                if (mat.doubleSided) {
                    glDisable(GL_CULL_FACE);
                } else {
                    glEnable(GL_CULL_FACE);
                }
            } else {
                // Fallback neutral material
                activeShader->setVec3("u_BaseColorFactor", glm::vec3(0.8f));
                activeShader->setFloat("u_AlphaFactor", 1.0f);
                activeShader->setFloat("u_MetallicFactor", 0.1f);
                activeShader->setFloat("u_RoughnessFactor", 0.5f);
                activeShader->setVec3("u_EmissiveFactor", glm::vec3(0.0f));
                activeShader->setBool("u_HasBaseColorTex", false);
                activeShader->setBool("u_HasNormalTex", false);
                activeShader->setBool("u_HasMetallicRoughnessTex", false);
                activeShader->setBool("u_HasEmissiveTex", false);
            }
        }

        mesh.draw();
    }
}
