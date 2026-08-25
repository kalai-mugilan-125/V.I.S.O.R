// ============================================================================
//  spatial_overlay.cpp  --  Spatial Overlay Facade & Transform Manager
// ============================================================================

#include "spatial_overlay.h"
#include "model_loader.h"
#include "shader_manager.h"

#include <glm/gtc/matrix_transform.hpp>

SpatialOverlay::SpatialOverlay()
    : m_model(std::make_unique<Model>()),
      m_rotation(0.0f),
      m_zoom(1.0f) {
}

SpatialOverlay::~SpatialOverlay() = default;

SpatialOverlay::SpatialOverlay(SpatialOverlay&& other) noexcept
    : m_model(std::move(other.m_model)),
      m_rotation(other.m_rotation),
      m_zoom(other.m_zoom),
      m_loadedFilePath(std::move(other.m_loadedFilePath)) {
}

SpatialOverlay& SpatialOverlay::operator=(SpatialOverlay&& other) noexcept {
    if (this != &other) {
        m_model = std::move(other.m_model);
        m_rotation = other.m_rotation;
        m_zoom = other.m_zoom;
        m_loadedFilePath = std::move(other.m_loadedFilePath);
    }
    return *this;
}

bool SpatialOverlay::loadModel(const std::string& filePath) {
    clear();
    auto newModel = std::make_unique<Model>();
    if (ModelLoader::load(filePath, *newModel)) {
        m_model = std::move(newModel);
        m_loadedFilePath = filePath;
        return true;
    }
    return false;
}

void SpatialOverlay::updateTransform(const glm::vec3& rotation, float zoom) {
    m_rotation = rotation;
    m_zoom     = zoom;
}

glm::mat4 SpatialOverlay::getModelMatrix() const {
    glm::mat4 modelMat = glm::mat4(1.0f);
    modelMat = glm::scale(modelMat, glm::vec3(m_zoom));
    modelMat = glm::rotate(modelMat, m_rotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
    modelMat = glm::rotate(modelMat, m_rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
    modelMat = glm::rotate(modelMat, m_rotation.z, glm::vec3(0.0f, 0.0f, 1.0f));

    if (m_model && !m_model->isEmpty()) {
        modelMat = modelMat * m_model->getNormalizationMatrix();
    }
    return modelMat;
}

void SpatialOverlay::draw(ShaderManager* activeShader) {
    if (!m_model || m_model->isEmpty()) return;
    m_model->draw(activeShader, getModelMatrix());
}

bool SpatialOverlay::isLoaded() const {
    return m_model && !m_model->isEmpty();
}

void SpatialOverlay::clear() {
    if (m_model) {
        m_model->clear();
    }
    m_loadedFilePath.clear();
}

size_t SpatialOverlay::getPrimitiveCount() const {
    return m_model ? m_model->getMeshCount() : 0;
}

size_t SpatialOverlay::getVertexCount() const {
    return m_model ? m_model->getVertexCount() : 0;
}

size_t SpatialOverlay::getTriangleCount() const {
    return m_model ? m_model->getTriangleCount() : 0;
}

size_t SpatialOverlay::getTextureCount() const {
    return m_model ? m_model->getTextureCount() : 0;
}

size_t SpatialOverlay::getMaterialCount() const {
    return m_model ? m_model->getMaterialCount() : 0;
}
