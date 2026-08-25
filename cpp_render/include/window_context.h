#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <string>

/**
 * @class WindowContext
 * @brief Manages the GLFW window lifecycle, OpenGL context, and input events.
 *
 * This is a standard decorated desktop window (with title bar, minimize,
 * maximize, and close buttons) suitable for running as a standalone
 * application.
 */
class WindowContext {
public:
  /**
   * @param width  Initial window width in pixels.
   * @param height Initial window height in pixels.
   * @param title  The title shown in the OS title bar.
   */
  WindowContext(int width = 1280, int height = 720, const char* title = "V.I.S.O.R. - 3D Spatial Viewer");
  ~WindowContext();

  /** Initialize GLFW, create the window, and load OpenGL via GLAD. */
  bool initialize();

  /** Swap the front and back render buffers (show the rendered frame). */
  void swapBuffers();

  /** Process all pending OS/window input events. */
  void pollEvents();

  /** Returns true when the user has requested the window to close. */
  bool shouldClose() const;

  /** Returns the raw GLFW window handle (needed by ImGui and callbacks). */
  GLFWwindow *getWindow() const;

  /** Returns the current framebuffer width (accounts for DPI scaling). */
  int getWidth() const;

  /** Returns the current framebuffer height (accounts for DPI scaling). */
  int getHeight() const;

private:
  int m_width;
  int m_height;
  const char *m_title;
  GLFWwindow *m_window;
};
