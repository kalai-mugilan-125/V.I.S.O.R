#include "camera.h"

Camera::Camera() 
    : m_target(0.0f, 0.0f, 0.0f), m_yaw(-90.0f), m_pitch(30.0f), m_zoom(15.0f) 
{}

void Camera::orbit(float dx, float dy) {
    m_yaw += dx;
    m_pitch += dy;
    
    // Clamp pitch to avoid gimbal lock and flipping
    if (m_pitch > 89.0f) m_pitch = 89.0f;
    if (m_pitch < -89.0f) m_pitch = -89.0f;
}

void Camera::pan(float dx, float dy) {
    float yawRad = glm::radians(m_yaw);
    // Right vector relative to the camera's yaw
    glm::vec3 right = glm::normalize(glm::vec3(cosf(yawRad - glm::half_pi<float>()), 0.0f, sinf(yawRad - glm::half_pi<float>())));
    glm::vec3 up    = glm::vec3(0.0f, 1.0f, 0.0f);
    
    // Scale pan speed based on zoom distance so it feels consistent
    float panSpeed = 0.01f * m_zoom;
    
    m_target += right * (dx * panSpeed) + up * (dy * panSpeed);
}

void Camera::zoom(float amount) {
    m_zoom += amount;
    if (m_zoom < 1.0f) m_zoom = 1.0f;
}

void Camera::reset() {
    m_target = glm::vec3(0.0f, 0.0f, 0.0f);
    m_yaw = -90.0f;
    m_pitch = 30.0f;
    m_zoom = 15.0f;
}

glm::vec3 Camera::getPosition() const {
    float yawRad   = glm::radians(m_yaw);
    float pitchRad = glm::radians(m_pitch);
    
    // Eye is calculated relative to the target using spherical coordinates
    return m_target + glm::vec3(
        m_zoom * cosf(pitchRad) * cosf(yawRad),
        m_zoom * sinf(pitchRad),
        m_zoom * cosf(pitchRad) * sinf(yawRad)
    );
}

glm::mat4 Camera::getViewMatrix() const {
    return glm::lookAt(getPosition(), m_target, glm::vec3(0.0f, 1.0f, 0.0f));
}
