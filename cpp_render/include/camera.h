#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

/**
 * @class Camera
 * @brief Encapsulates a Maya/Blender style orbital camera.
 */
class Camera {
public:
    Camera();
    ~Camera() = default;

    /**
     * @brief Orbit the camera around its target.
     * @param dx Horizontal rotation delta
     * @param dy Vertical rotation delta
     */
    void orbit(float dx, float dy);

    /**
     * @brief Pan the camera's target parallel to the screen plane.
     * @param dx Horizontal movement
     * @param dy Vertical movement
     */
    void pan(float dx, float dy);

    /**
     * @brief Zoom the camera in or out towards its target.
     * @param amount Zoom delta (positive zooms out, negative zooms in)
     */
    void zoom(float amount);

    /**
     * @brief Reset the camera to its default orientation and distance.
     */
    void reset();

    /**
     * @brief Get the current view matrix for rendering.
     */
    glm::mat4 getViewMatrix() const;

    /**
     * @brief Get the absolute world position of the camera.
     */
    glm::vec3 getPosition() const;

    // Direct accessors if needed
    float getZoom() const { return m_zoom; }
    glm::vec3 getTarget() const { return m_target; }

private:
    glm::vec3 m_target;
    float m_yaw;
    float m_pitch;
    float m_zoom;
};
