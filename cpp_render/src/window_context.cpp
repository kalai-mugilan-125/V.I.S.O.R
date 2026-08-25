#include "window_context.h"
#include <iostream>

/**
 * @brief GLFW framebuffer resize callback.
 * Called automatically by GLFW when the user resizes the window.
 * Updates the OpenGL viewport to match the new dimensions.
 */
static void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

WindowContext::WindowContext(int width, int height, const char* title)
    : m_width(width), m_height(height), m_title(title), m_window(nullptr) {
}

WindowContext::~WindowContext() {
    // Destroy the GLFW window and clean up all GLFW resources
    if (m_window) {
        glfwDestroyWindow(m_window);
    }
    glfwTerminate();
}

bool WindowContext::initialize() {
    // --- Initialize GLFW ---
    if (!glfwInit()) {
        std::cerr << "[WindowContext] Failed to initialize GLFW." << std::endl;
        return false;
    }

    // --- Request OpenGL 3.3 Core Profile ---
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // --- Standard decorated window (title bar, minimize, resize, close buttons) ---
    // GLFW_DECORATED = TRUE  -> OS provides the standard title bar and window controls.
    // GLFW_RESIZABLE  = TRUE -> The user can freely resize the window.
    glfwWindowHint(GLFW_DECORATED, GLFW_TRUE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);

    // --- Create the window ---
    m_window = glfwCreateWindow(m_width, m_height, m_title, nullptr, nullptr);
    if (!m_window) {
        std::cerr << "[WindowContext] Failed to create GLFW window." << std::endl;
        glfwTerminate();
        return false;
    }

    // Make the new window's OpenGL context the current one on this thread
    glfwMakeContextCurrent(m_window);

    // Enable V-Sync to cap frame rate and prevent screen tearing
    glfwSwapInterval(1);

    // --- Load OpenGL function pointers via GLAD ---
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "[WindowContext] Failed to initialize GLAD." << std::endl;
        return false;
    }

    // --- Set the initial viewport to cover the full window ---
    int actualW = 0, actualH = 0;
    glfwGetFramebufferSize(m_window, &actualW, &actualH);
    glViewport(0, 0, actualW, actualH);

    // Register the resize callback so the viewport updates when the window is resized
    glfwSetFramebufferSizeCallback(m_window, framebuffer_size_callback);

    std::cout << "[WindowContext] Window initialized: " << actualW << "x" << actualH << std::endl;
    std::cout << "[WindowContext] OpenGL Version: " << glGetString(GL_VERSION) << std::endl;
    return true;
}

void WindowContext::swapBuffers() {
    glfwSwapBuffers(m_window);
}

void WindowContext::pollEvents() {
    glfwPollEvents();
}

bool WindowContext::shouldClose() const {
    return glfwWindowShouldClose(m_window);
}

GLFWwindow* WindowContext::getWindow() const {
    return m_window;
}

int WindowContext::getWidth() const {
    int w, h;
    glfwGetFramebufferSize(m_window, &w, &h);
    return w;
}

int WindowContext::getHeight() const {
    int w, h;
    glfwGetFramebufferSize(m_window, &w, &h);
    return h;
}
