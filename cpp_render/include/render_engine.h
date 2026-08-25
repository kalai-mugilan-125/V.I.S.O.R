#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <map>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

// Forward declarations to avoid circular header includes
class WindowContext;
class SpatialOverlay;
class UDPReceiver;
class ShaderManager;
class Camera;
class GestureController;

/**
 * @class RenderEngine
 * @brief Top-level orchestrator of the entire rendering pipeline.
 *
 * Matches the 'RenderEngine' class in the V.I.S.O.R. UML class diagram.
 * Manages:
 *   - WindowContext  : GLFW window and OpenGL context.
 *   - SpatialOverlay : The 3D model and its transforms.
 *   - UDPReceiver    : Async data stream from the Python AI microservice.
 *   - ShaderManager  : Compilation and use of GLSL shaders.
 *   - AI Service     : Spawns and manages headless background AI process.
 *
 * Also owns the infinite XY grid rendering resources (VAO/VBO).
 */
class RenderEngine {
public:
    RenderEngine();
    ~RenderEngine();

    /** Initialize all subsystems (window, shaders, UDP, ImGui, grid geometry, background AI). */
    bool initialize();

    /** Enter the main render loop. Blocks until the window is closed. */
    void runRenderLoop();

private:
    // --- Subsystem instances ---
    std::unique_ptr<WindowContext>  m_windowContext;
    std::unique_ptr<SpatialOverlay> m_spatialOverlay;
    std::unique_ptr<UDPReceiver>    m_udpReceiver;
    
    // Shader Management
    std::map<std::string, std::unique_ptr<ShaderManager>> m_shaders;
    std::string m_activeShaderName;

    // UI State
    bool m_showSceneControls; ///< Toggle for the right-side properties panel

    /**
     * @brief Renders the infinite XY grid using a full-screen quad technique.
     */
    void renderGrid(const glm::mat4& view, const glm::mat4& projection);

    /**
     * @brief Renders a visual representation of the light source.
     */
    void renderLightObject(const glm::mat4& view, const glm::mat4& projection);

    // --- Grid Rendering State ---
    GLuint gridVAO; ///< Vertex Array Object for the full-screen grid quad
    GLuint gridVBO; ///< Vertex Buffer Object for the grid quad vertices

    // --- Light Rendering State ---
    GLuint lightVAO; ///< Vertex Array Object for the light cube
    GLuint lightVBO; ///< Vertex Buffer Object for the light cube
    glm::vec3 m_lightPos; ///< Current light position in world space

    // OOP Encapsulation (Memory Safety)
    std::unique_ptr<Camera> m_camera;
    std::unique_ptr<GestureController> m_gestureController;

    // Mouse Tracking for Keyboard/Mouse Fallback
    float m_lastMouseX;  
    float m_lastMouseY;  
    bool  m_firstMouse;

    // Background AI Process Management
    bool m_aiProcessStarted;
    std::string m_aiTempExePath; // Path of extracted temp exe to clean up on exit
#ifdef _WIN32
    PROCESS_INFORMATION m_aiProcessInfo;
#endif
    void launchAIService();
    void stopAIService();

    /** Open a native Windows file dialog and return the selected file path. */
    std::string openFileDialog();
};
