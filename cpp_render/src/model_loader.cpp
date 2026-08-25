#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <tiny_gltf.h>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#include "model_loader.h"
#include "model.h"
#include "mesh.h"
#include "material.h"
#include "texture.h"

#include <iostream>
#include <algorithm>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>

bool ModelLoader::load(const std::string& filePath, Model& outModel) {
    if (filePath.empty()) return false;

    outModel.clear();

    std::string ext;
    size_t dotPos = filePath.find_last_of('.');
    if (dotPos != std::string::npos) {
        ext = filePath.substr(dotPos);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    }

    bool success = false;
    if (ext == ".gltf" || ext == ".glb") {
        success = loadGLTF(filePath, outModel);
    } else if (ext == ".obj") {
        success = loadOBJ(filePath, outModel);
    } else {
        std::cerr << "[ModelLoader] Unsupported format: " << ext << std::endl;
        return false;
    }

    if (success) {
        outModel.computeBoundsAndNormalization();
        std::cout << "[ModelLoader] Loaded '" << filePath << "' successfully: "
                  << outModel.getMeshCount() << " meshes, "
                  << outModel.getVertexCount() << " vertices, "
                  << outModel.getTriangleCount() << " triangles, "
                  << outModel.getTextureCount() << " textures, "
                  << outModel.getMaterialCount() << " materials." << std::endl;
    }

    return success;
}

bool ModelLoader::loadGLTF(const std::string& filePath, Model& outModel) {
    tinygltf::Model gltfModel;
    tinygltf::TinyGLTF loader;
    std::string err, warn;

    bool isBinary = (filePath.size() >= 4 && filePath.substr(filePath.size() - 4) == ".glb");
    bool ret = isBinary ? loader.LoadBinaryFromFile(&gltfModel, &err, &warn, filePath)
                        : loader.LoadASCIIFromFile(&gltfModel, &err, &warn, filePath);

    if (!warn.empty()) std::cout << "[ModelLoader glTF Warn] " << warn << std::endl;
    if (!err.empty())  std::cerr << "[ModelLoader glTF Error] " << err << std::endl;
    if (!ret) return false;

    // 1. Process Textures
    for (size_t i = 0; i < gltfModel.textures.size(); ++i) {
        const auto& gltfTex = gltfModel.textures[i];
        if (gltfTex.source < 0 || gltfTex.source >= (int)gltfModel.images.size()) {
            outModel.addTexture(nullptr);
            continue;
        }

        const auto& img = gltfModel.images[gltfTex.source];
        auto texObj = std::make_unique<Texture>();

        GLint wrapS = GL_REPEAT;
        GLint wrapT = GL_REPEAT;
        GLint minFilter = GL_LINEAR_MIPMAP_LINEAR;
        GLint magFilter = GL_LINEAR;

        if (gltfTex.sampler >= 0 && gltfTex.sampler < (int)gltfModel.samplers.size()) {
            const auto& s = gltfModel.samplers[gltfTex.sampler];
            if (s.wrapS) wrapS = s.wrapS;
            if (s.wrapT) wrapT = s.wrapT;
            if (s.minFilter) minFilter = s.minFilter;
            if (s.magFilter) magFilter = s.magFilter;
        }

        if (!img.image.empty()) {
            texObj->loadFromDecodedPixels(img.image.data(), img.width, img.height, img.component,
                                          true, wrapS, wrapT, minFilter, magFilter);
        }
        outModel.addTexture(std::move(texObj));
    }

    // 2. Process Materials
    for (const auto& gltfMat : gltfModel.materials) {
        Material mat{};
        mat.name = gltfMat.name;
        mat.doubleSided = gltfMat.doubleSided;
        mat.alphaMode = gltfMat.alphaMode;
        mat.alphaCutoff = (float)gltfMat.alphaCutoff;

        const auto& pbr = gltfMat.pbrMetallicRoughness;
        mat.baseColorFactor = glm::vec4(
            (float)pbr.baseColorFactor[0],
            (float)pbr.baseColorFactor[1],
            (float)pbr.baseColorFactor[2],
            (float)pbr.baseColorFactor[3]
        );
        mat.metallicFactor = (float)pbr.metallicFactor;
        mat.roughnessFactor = (float)pbr.roughnessFactor;

        mat.baseColorTextureIndex = pbr.baseColorTexture.index;
        mat.metallicRoughnessTextureIndex = pbr.metallicRoughnessTexture.index;
        mat.normalTextureIndex = gltfMat.normalTexture.index;
        mat.emissiveTextureIndex = gltfMat.emissiveTexture.index;
        mat.occlusionTextureIndex = gltfMat.occlusionTexture.index;

        mat.emissiveFactor = glm::vec3(
            (float)gltfMat.emissiveFactor[0],
            (float)gltfMat.emissiveFactor[1],
            (float)gltfMat.emissiveFactor[2]
        );

        outModel.addMaterial(mat);
    }

    if (outModel.getMaterialCount() == 0) {
        Material defaultMat{};
        defaultMat.name = "Default";
        outModel.addMaterial(defaultMat);
    }

    // 3. Process Node Hierarchy & Submeshes
    auto processNode = [&](auto& self, int nodeIdx, const glm::mat4& parentMatrix) -> void {
        if (nodeIdx < 0 || nodeIdx >= (int)gltfModel.nodes.size()) return;
        const auto& node = gltfModel.nodes[nodeIdx];

        glm::mat4 localMatrix(1.0f);
        if (node.matrix.size() == 16) {
            localMatrix = glm::make_mat4(node.matrix.data());
        } else {
            if (node.translation.size() == 3) {
                localMatrix = glm::translate(localMatrix, glm::vec3((float)node.translation[0], (float)node.translation[1], (float)node.translation[2]));
            }
            if (node.rotation.size() == 4) {
                glm::quat q((float)node.rotation[3], (float)node.rotation[0], (float)node.rotation[1], (float)node.rotation[2]);
                localMatrix *= glm::toMat4(q);
            }
            if (node.scale.size() == 3) {
                localMatrix = glm::scale(localMatrix, glm::vec3((float)node.scale[0], (float)node.scale[1], (float)node.scale[2]));
            }
        }

        glm::mat4 worldMatrix = parentMatrix * localMatrix;

        if (node.mesh >= 0 && node.mesh < (int)gltfModel.meshes.size()) {
            const auto& mesh = gltfModel.meshes[node.mesh];
            for (const auto& primitive : mesh.primitives) {
                if (primitive.attributes.find("POSITION") == primitive.attributes.end()) continue;

                // Positions
                const auto& posAcc = gltfModel.accessors[primitive.attributes.at("POSITION")];
                const auto& posBv  = gltfModel.bufferViews[posAcc.bufferView];
                const auto& posBuf = gltfModel.buffers[posBv.buffer];
                const float* posData = reinterpret_cast<const float*>(&posBuf.data[posBv.byteOffset + posAcc.byteOffset]);
                size_t posStride = posBv.byteStride ? (posBv.byteStride / sizeof(float)) : 3;

                // Normals
                const float* normData = nullptr;
                size_t normStride = 3;
                if (primitive.attributes.find("NORMAL") != primitive.attributes.end()) {
                    const auto& normAcc = gltfModel.accessors[primitive.attributes.at("NORMAL")];
                    const auto& normBv  = gltfModel.bufferViews[normAcc.bufferView];
                    const auto& normBuf = gltfModel.buffers[normBv.buffer];
                    normData = reinterpret_cast<const float*>(&normBuf.data[normBv.byteOffset + normAcc.byteOffset]);
                    normStride = normBv.byteStride ? (normBv.byteStride / sizeof(float)) : 3;
                }

                // TexCoords
                const float* texData = nullptr;
                size_t texStride = 2;
                if (primitive.attributes.find("TEXCOORD_0") != primitive.attributes.end()) {
                    const auto& texAcc = gltfModel.accessors[primitive.attributes.at("TEXCOORD_0")];
                    const auto& texBv  = gltfModel.bufferViews[texAcc.bufferView];
                    const auto& texBuf = gltfModel.buffers[texBv.buffer];
                    texData = reinterpret_cast<const float*>(&texBuf.data[texBv.byteOffset + texAcc.byteOffset]);
                    texStride = texBv.byteStride ? (texBv.byteStride / sizeof(float)) : 2;
                }

                // Tangents
                const float* tanData = nullptr;
                size_t tanStride = 4;
                if (primitive.attributes.find("TANGENT") != primitive.attributes.end()) {
                    const auto& tanAcc = gltfModel.accessors[primitive.attributes.at("TANGENT")];
                    const auto& tanBv  = gltfModel.bufferViews[tanAcc.bufferView];
                    const auto& tanBuf = gltfModel.buffers[tanBv.buffer];
                    tanData = reinterpret_cast<const float*>(&tanBuf.data[tanBv.byteOffset + tanAcc.byteOffset]);
                    tanStride = tanBv.byteStride ? (tanBv.byteStride / sizeof(float)) : 4;
                }

                std::vector<Vertex> vertices(posAcc.count);
                for (size_t v = 0; v < posAcc.count; ++v) {
                    Vertex vert{};
                    vert.position = glm::vec3(posData[v * posStride + 0], posData[v * posStride + 1], posData[v * posStride + 2]);
                    vert.normal   = normData ? glm::vec3(normData[v * normStride + 0], normData[v * normStride + 1], normData[v * normStride + 2]) : glm::vec3(0.0f, 1.0f, 0.0f);
                    vert.texcoord = texData ? glm::vec2(texData[v * texStride + 0], texData[v * texStride + 1]) : glm::vec2(0.0f, 0.0f);
                    vert.tangent  = tanData ? glm::vec4(tanData[v * tanStride + 0], tanData[v * tanStride + 1], tanData[v * tanStride + 2], tanData[v * tanStride + 3]) : glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
                    vertices[v] = vert;
                }

                // Indices
                std::vector<unsigned int> indices;
                if (primitive.indices >= 0) {
                    const auto& indAcc = gltfModel.accessors[primitive.indices];
                    const auto& indBv  = gltfModel.bufferViews[indAcc.bufferView];
                    const auto& indBuf = gltfModel.buffers[indBv.buffer];
                    const unsigned char* rawInd = &indBuf.data[indBv.byteOffset + indAcc.byteOffset];

                    indices.resize(indAcc.count);
                    if (indAcc.componentType == TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT) {
                        const uint32_t* buf = reinterpret_cast<const uint32_t*>(rawInd);
                        for (size_t i = 0; i < indAcc.count; ++i) indices[i] = buf[i];
                    } else if (indAcc.componentType == TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT) {
                        const uint16_t* buf = reinterpret_cast<const uint16_t*>(rawInd);
                        for (size_t i = 0; i < indAcc.count; ++i) indices[i] = buf[i];
                    } else if (indAcc.componentType == TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE) {
                        const uint8_t* buf = reinterpret_cast<const uint8_t*>(rawInd);
                        for (size_t i = 0; i < indAcc.count; ++i) indices[i] = buf[i];
                    }
                }

                int matIndex = primitive.material >= 0 ? primitive.material : 0;
                Mesh meshObj(std::move(vertices), std::move(indices), matIndex, worldMatrix);
                outModel.addMesh(std::move(meshObj));
            }
        }

        for (int childIdx : node.children) {
            self(self, childIdx, worldMatrix);
        }
    };

    int sceneIdx = gltfModel.defaultScene >= 0 ? gltfModel.defaultScene : 0;
    if (sceneIdx < (int)gltfModel.scenes.size()) {
        for (int rootNode : gltfModel.scenes[sceneIdx].nodes) {
            processNode(processNode, rootNode, glm::mat4(1.0f));
        }
    } else {
        for (size_t i = 0; i < gltfModel.nodes.size(); ++i) {
            processNode(processNode, (int)i, glm::mat4(1.0f));
        }
    }

    return !outModel.isEmpty();
}

bool ModelLoader::loadOBJ(const std::string& filePath, Model& outModel) {
    tinyobj::attrib_t                attrib;
    std::vector<tinyobj::shape_t>    shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filePath.c_str())) {
        std::cerr << "[ModelLoader] TinyOBJ error: " << err << std::endl;
        return false;
    }

    Material defaultMat{};
    defaultMat.name = "OBJ_Material";
    defaultMat.baseColorFactor = glm::vec4(0.7f, 0.7f, 0.7f, 1.0f);
    defaultMat.metallicFactor = 0.1f;
    defaultMat.roughnessFactor = 0.5f;
    outModel.addMaterial(defaultMat);

    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;

    for (const auto& shape : shapes) {
        for (const auto& index : shape.mesh.indices) {
            Vertex vertex{};
            vertex.position = {
                attrib.vertices[3 * index.vertex_index + 0],
                attrib.vertices[3 * index.vertex_index + 1],
                attrib.vertices[3 * index.vertex_index + 2]
            };

            if (index.normal_index >= 0) {
                vertex.normal = {
                    attrib.normals[3 * index.normal_index + 0],
                    attrib.normals[3 * index.normal_index + 1],
                    attrib.normals[3 * index.normal_index + 2]
                };
            } else {
                vertex.normal = {0.0f, 1.0f, 0.0f};
            }

            if (index.texcoord_index >= 0) {
                vertex.texcoord = {
                    attrib.texcoords[2 * index.texcoord_index + 0],
                    1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
                };
            } else {
                vertex.texcoord = {0.0f, 0.0f};
            }

            vertex.tangent = {1.0f, 0.0f, 0.0f, 1.0f};

            vertices.push_back(vertex);
            indices.push_back((unsigned int)indices.size());
        }
    }

    if (vertices.empty()) return false;

    Mesh meshObj(std::move(vertices), std::move(indices), 0, glm::mat4(1.0f));
    outModel.addMesh(std::move(meshObj));
    return true;
}
