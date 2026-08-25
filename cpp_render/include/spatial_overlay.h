#pragma once

#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <vector>

#include "model.h"

// Forward declaration
class ShaderManager;

/**
 * @class SpatialOverlay
 * @brief High-level spatial entity manager representing the 3D model in the scene.
 *
 * Encapsulates:
 *   - Ownership of the loaded Model (meshes, materials, textures)
 *   - Spatial gesture transforms (Euler rotation, pinch zoom scaling)
 *   - Facade for model loading and rendering
 */
class SpatialOverlay {
public:
    SpatialOverlay();
    ~SpatialOverlay();

    // Disable copy
    SpatialOverlay(const SpatialOverlay&) = delete;
    SpatialOverlay& operator=(const SpatialOverlay&) = delete;

    // Move semantics
    SpatialOverlay(SpatialOverlay&& other) noexcept;
    SpatialOverlay& operator=(SpatialOverlay&& other) noexcept;

    /**
     * @brief Load a 3D model from disk via ModelLoader.
     * @param filePath Path to .gltf, .glb, or .obj file.
     * @return true on success, false on failure.
     */
    bool loadModel(const std::string& filePath);

    /**
     * @brief Update world-space interaction transform.
     * @param rotation Euler angles (radians) for X/Y/Z rotation.
     * @param zoom     Uniform scale factor.
     */
    void updateTransform(const glm::vec3& rotation, float zoom);

    /**
     * @brief Render the loaded model using the active shader pipeline.
     * @param activeShader Pointer to the active ShaderManager.
     */
    void draw(ShaderManager* activeShader = nullptr);

    /** Returns the composed 4x4 model matrix (zoom * rotation * normalization). */
    glm::mat4 getModelMatrix() const;

    /** Returns true if a model is currently loaded. */
    bool isLoaded() const;

    /** Releases loaded model resources. */
    void clear();

    // --- Model Statistics ---
    size_t getPrimitiveCount() const;
    size_t getVertexCount() const;
    size_t getTriangleCount() const;
    size_t getTextureCount() const;
    size_t getMaterialCount() const;
    const std::string& getLoadedFilePath() const { return m_loadedFilePath; }

    const Model* getModel() const { return m_model.get(); }

private:
    std::unique_ptr<Model> m_model;

    glm::vec3   m_rotation;
    float       m_zoom;
    std::string m_loadedFilePath;
};
