#pragma once

#include <glm/glm.hpp>
#include <string>

/**
 * @struct Material
 * @brief Represents a PBR Metallic-Roughness material corresponding to glTF 2.0 specifications.
 */
struct Material {
    std::string name;

    // Material Factors
    glm::vec4 baseColorFactor = glm::vec4(1.0f);
    float     metallicFactor  = 1.0f;
    float     roughnessFactor = 1.0f;
    glm::vec3 emissiveFactor  = glm::vec3(0.0f);

    // Texture indices (-1 indicates no texture attached)
    int baseColorTextureIndex          = -1;
    int normalTextureIndex             = -1;
    int metallicRoughnessTextureIndex  = -1;
    int emissiveTextureIndex           = -1;
    int occlusionTextureIndex          = -1;

    // Rendering configuration
    bool        doubleSided = false;
    std::string alphaMode   = "OPAQUE"; // OPAQUE, MASK, BLEND
    float       alphaCutoff = 0.5f;
};
