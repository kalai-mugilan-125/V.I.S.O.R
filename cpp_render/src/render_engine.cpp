// ============================================================================
//  render_engine.cpp  --  V.I.S.O.R. Native 3D Render Engine
//  Implements the main rendering pipeline: infinite grid, OBJ model,
//  ImGui toolbar, orbit camera, and UDP data from the Python microservice.
// ============================================================================

#include "render_engine.h"
#include "camera.h"
#include "embedded_shaders.h"
#include "gesture_controller.h"
#include "shader_manager.h"
#include "spatial_overlay.h"
#include "udp_server.h"
#include "window_context.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <iostream>
#include <string>

// Windows native file dialog (only on Windows)
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <commdlg.h> // For GetOpenFileName()
#include <windows.h>
#pragma comment(lib, "comdlg32.lib")
#endif

// -----------------------------------------------------------------------------
// Constructor & Destructor
// -----------------------------------------------------------------------------
RenderEngine::RenderEngine()
    : m_activeShaderName("Blinn-Phong"), m_showSceneControls(true), gridVAO(0),
      gridVBO(0), lightVAO(0), lightVBO(0), m_lightPos(2.0f, 2.0f, 2.0f),
      m_lastMouseX(0.0f), m_lastMouseY(0.0f), m_firstMouse(true),
      m_aiProcessStarted(false),
      m_aiTempExePath("") {
  m_windowContext = std::make_unique<WindowContext>();
  m_spatialOverlay = std::make_unique<SpatialOverlay>();
  m_udpReceiver = std::make_unique<UDPReceiver>(5005);

  m_camera = std::make_unique<Camera>();
  m_gestureController = std::make_unique<GestureController>();
}

RenderEngine::~RenderEngine() {
  // Terminate background AI service if we started it
  stopAIService();

  // Cleanly shut down ImGui before GLFW/OpenGL context is destroyed
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  // Release grid and light GPU resources
  if (gridVAO)
    glDeleteVertexArrays(1, &gridVAO);
  if (gridVBO)
    glDeleteBuffers(1, &gridVBO);
  if (lightVAO)
    glDeleteVertexArrays(1, &lightVAO);
  if (lightVBO)
    glDeleteBuffers(1, &lightVBO);
}

void RenderEngine::launchAIService() {
#ifdef _WIN32
    std::string exeToLaunch = "";

    // --- Step 1: Try to extract visor_ai.exe from embedded Windows resource ---
    // The resource ID 1001 is defined in visor_ai.rc during the packaging build.
    HRSRC hRes = FindResourceA(NULL, MAKEINTRESOURCEA(1001), "AI_BINARY");
    if (hRes != NULL) {
        HGLOBAL hData = LoadResource(NULL, hRes);
        if (hData != NULL) {
            DWORD dataSize = SizeofResource(NULL, hRes);
            void* pData = LockResource(hData);
            if (pData != NULL && dataSize > 0) {
                // Write to %TEMP%\visor_ai.exe
                char tempPath[MAX_PATH];
                GetTempPathA(MAX_PATH, tempPath);
                std::string tempExe = std::string(tempPath) + "visor_ai.exe";

                HANDLE hFile = CreateFileA(tempExe.c_str(), GENERIC_WRITE, 0, NULL,
                                           CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
                if (hFile != INVALID_HANDLE_VALUE) {
                    DWORD written = 0;
                    WriteFile(hFile, pData, dataSize, &written, NULL);
                    CloseHandle(hFile);
                    exeToLaunch = tempExe;
                    m_aiTempExePath = tempExe; // Remember for cleanup
                    std::cout << "[RenderEngine] Extracted embedded AI service to: " << tempExe << std::endl;
                }
            }
        }
    }

    // --- Step 2: Fallback to searching for visor_ai.exe on disk ---
    if (exeToLaunch.empty()) {
        const char* candidatePaths[] = { "visor_ai.exe", "./visor_ai.exe", "../visor_ai.exe" };
        for (const auto& path : candidatePaths) {
            DWORD attrib = GetFileAttributesA(path);
            if (attrib != INVALID_FILE_ATTRIBUTES && !(attrib & FILE_ATTRIBUTE_DIRECTORY)) {
                exeToLaunch = path;
                break;
            }
        }
    }

    // --- Step 3: Launch the AI service ---
    if (!exeToLaunch.empty()) {
        STARTUPINFOA si;
        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        ZeroMemory(&m_aiProcessInfo, sizeof(m_aiProcessInfo));

        BOOL success = CreateProcessA(
            exeToLaunch.c_str(), NULL, NULL, NULL,
            FALSE, CREATE_NO_WINDOW, NULL, NULL,
            &si, &m_aiProcessInfo
        );

        if (success) {
            m_aiProcessStarted = true;
            std::cout << "[RenderEngine] Background AI service active." << std::endl;
        } else {
            std::cerr << "[RenderEngine] Failed to launch AI service (error: " << GetLastError() << ")" << std::endl;
        }
    } else {
        std::cout << "[RenderEngine] No AI service found — gesture control disabled." << std::endl;
    }
#endif
}

void RenderEngine::stopAIService() {
#ifdef _WIN32
  if (m_aiProcessStarted) {
      std::cout << "[RenderEngine] Terminating background AI service..." << std::endl;
      TerminateProcess(m_aiProcessInfo.hProcess, 0);
      WaitForSingleObject(m_aiProcessInfo.hProcess, 2000); // Wait up to 2s for clean exit
      CloseHandle(m_aiProcessInfo.hProcess);
      CloseHandle(m_aiProcessInfo.hThread);
      m_aiProcessStarted = false;

      // Delete the temp extracted binary to keep %TEMP% clean
      if (!m_aiTempExePath.empty()) {
          DeleteFileA(m_aiTempExePath.c_str());
          m_aiTempExePath.clear();
      }
  }
#endif
}

// ---------------------------------------------------------------------------
//  initialize()  --  Setup window, shaders, ImGui, and geometries
// ---------------------------------------------------------------------------

bool RenderEngine::initialize() {
  // 1. Launch headless background AI service if present
  launchAIService();

  // 2. Create the OS window and OpenGL context
  if (!m_windowContext->initialize()) {
    std::cerr << "[RenderEngine] Window initialization failed." << std::endl;
    return false;
  }

  // 3. Start listening for UDP data from the Python AI microservice
  // (non-blocking)
  if (!m_udpReceiver->initialize()) {
    std::cerr << "[RenderEngine] UDP receiver initialization failed."
              << std::endl;
    return false;
  }

  // 3. Compile and link all GLSL shaders (from embedded source - no files needed)
  m_shaders["Holographic"] = std::make_unique<ShaderManager>();
  m_shaders["Holographic"]->compileFromSource(Shaders::DEFAULT_VERT, Shaders::HOLOGRAPHIC_FRAG);

  m_shaders["Normals Debug"] = std::make_unique<ShaderManager>();
  m_shaders["Normals Debug"]->compileFromSource(Shaders::DEFAULT_VERT, Shaders::NORMALS_FRAG);

  m_shaders["Blinn-Phong"] = std::make_unique<ShaderManager>();
  m_shaders["Blinn-Phong"]->compileFromSource(Shaders::DEFAULT_VERT, Shaders::BLINN_PHONG_FRAG);

  m_shaders["Toon Shading"] = std::make_unique<ShaderManager>();
  m_shaders["Toon Shading"]->compileFromSource(Shaders::DEFAULT_VERT, Shaders::TOON_FRAG);

  m_shaders["PBR (Simplified)"] = std::make_unique<ShaderManager>();
  m_shaders["PBR (Simplified)"]->compileFromSource(Shaders::DEFAULT_VERT, Shaders::PBR_FRAG);

  m_shaders["Grid"] = std::make_unique<ShaderManager>();
  m_shaders["Grid"]->compileFromSource(Shaders::GRID_VERT, Shaders::GRID_FRAG);

  m_shaders["Light"] = std::make_unique<ShaderManager>();
  m_shaders["Light"]->compileFromSource(Shaders::DEFAULT_VERT, Shaders::LIGHT_FRAG);

  // 4. Initialize Dear ImGui (UI toolkit for the toolbar menu)
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  io.ConfigFlags |=
      ImGuiConfigFlags_NavEnableKeyboard; // Enable keyboard navigation

  ImGui::StyleColorsDark();
  ImGuiStyle &style = ImGui::GetStyle();
  style.WindowRounding = 0.0f; // Flat style for side panel

  ImGui_ImplGlfw_InitForOpenGL(m_windowContext->getWindow(), true);
  ImGui_ImplOpenGL3_Init("#version 330");

  // 5. Build the grid geometry: a full-screen clip-space quad
  float gridVertices[] = {-1.0f, -1.0f, 0.0f, 1.0f,  -1.0f, 0.0f,
                          1.0f,  1.0f,  0.0f, -1.0f, 1.0f,  0.0f};
  glGenVertexArrays(1, &gridVAO);
  glGenBuffers(1, &gridVBO);
  glBindVertexArray(gridVAO);
  glBindBuffer(GL_ARRAY_BUFFER, gridVBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(gridVertices), gridVertices,
               GL_STATIC_DRAW);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
  glBindVertexArray(0);

  // 6. Build a simple cube geometry for the Light object
  float lightVertices[] = {
      // Back face
      -0.1f, -0.1f, -0.1f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.1f, -0.1f, -0.1f,
      0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.1f, 0.1f, -0.1f, 0.0f, 0.0f, -1.0f, 1.0f,
      1.0f, 0.1f, 0.1f, -0.1f, 0.0f, 0.0f, -1.0f, 1.0f, 1.0f, -0.1f, 0.1f,
      -0.1f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f, -0.1f, -0.1f, -0.1f, 0.0f, 0.0f,
      -1.0f, 0.0f, 0.0f,
      // Front face
      -0.1f, -0.1f, 0.1f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.1f, -0.1f, 0.1f, 0.0f,
      0.0f, 1.0f, 1.0f, 0.0f, 0.1f, 0.1f, 0.1f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,
      0.1f, 0.1f, 0.1f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, -0.1f, 0.1f, 0.1f, 0.0f,
      0.0f, 1.0f, 0.0f, 1.0f, -0.1f, -0.1f, 0.1f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
      // Left face
      -0.1f, 0.1f, 0.1f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f, -0.1f, 0.1f, -0.1f,
      -1.0f, 0.0f, 0.0f, 1.0f, 1.0f, -0.1f, -0.1f, -0.1f, -1.0f, 0.0f, 0.0f,
      0.0f, 1.0f, -0.1f, -0.1f, -0.1f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, -0.1f,
      -0.1f, 0.1f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -0.1f, 0.1f, 0.1f, -1.0f,
      0.0f, 0.0f, 1.0f, 0.0f,
      // Right face
      0.1f, 0.1f, 0.1f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.1f, 0.1f, -0.1f, 1.0f,
      0.0f, 0.0f, 1.0f, 1.0f, 0.1f, -0.1f, -0.1f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f,
      0.1f, -0.1f, -0.1f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.1f, -0.1f, 0.1f, 1.0f,
      0.0f, 0.0f, 0.0f, 0.0f, 0.1f, 0.1f, 0.1f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f,
      // Bottom face
      -0.1f, -0.1f, -0.1f, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0.1f, -0.1f, -0.1f,
      0.0f, -1.0f, 0.0f, 1.0f, 1.0f, 0.1f, -0.1f, 0.1f, 0.0f, -1.0f, 0.0f, 1.0f,
      0.0f, 0.1f, -0.1f, 0.1f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f, -0.1f, -0.1f,
      0.1f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, -0.1f, -0.1f, -0.1f, 0.0f, -1.0f,
      0.0f, 0.0f, 1.0f,
      // Top face
      -0.1f, 0.1f, -0.1f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.1f, 0.1f, -0.1f, 0.0f,
      1.0f, 0.0f, 1.0f, 1.0f, 0.1f, 0.1f, 0.1f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f,
      0.1f, 0.1f, 0.1f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, -0.1f, 0.1f, 0.1f, 0.0f,
      1.0f, 0.0f, 0.0f, 0.0f, -0.1f, 0.1f, -0.1f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f};
  glGenVertexArrays(1, &lightVAO);
  glGenBuffers(1, &lightVBO);
  glBindVertexArray(lightVAO);
  glBindBuffer(GL_ARRAY_BUFFER, lightVBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(lightVertices), lightVertices,
               GL_STATIC_DRAW);

  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)0);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float),
                        (void *)(3 * sizeof(float)));
  glEnableVertexAttribArray(2);
  glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float),
                        (void *)(6 * sizeof(float)));
  glBindVertexArray(0);

  std::cout << "[RenderEngine] Initialization complete." << std::endl;
  return true;
}

// ---------------------------------------------------------------------------
//  openFileDialog()  --  Native Windows file picker
// ---------------------------------------------------------------------------

std::string RenderEngine::openFileDialog() {
#ifdef _WIN32
  char filePath[MAX_PATH] = {0};

  OPENFILENAMEA ofn;
  ZeroMemory(&ofn, sizeof(ofn));
  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = NULL;
  ofn.lpstrFile = filePath;
  ofn.nMaxFile = MAX_PATH;
  ofn.lpstrFilter = "3D Models (*.gltf;*.glb;*.obj)\0*.gltf;*.glb;*.obj\0glTF 2.0 (*.gltf;*.glb)\0*.gltf;*.glb\0Wavefront OBJ (*.obj)\0*.obj\0All Files (*.*)\0*.*\0";
  ofn.nFilterIndex = 1;
  ofn.lpstrTitle = "Open 3D Model";
  ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

  if (GetOpenFileNameA(&ofn)) {
    return std::string(filePath);
  }
#endif
  return "";
}

// ---------------------------------------------------------------------------
//  renderGrid()  --  Draw the infinite XY ground plane
// ---------------------------------------------------------------------------

void RenderEngine::renderGrid(const glm::mat4 &view,
                              const glm::mat4 &projection) {
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDepthMask(GL_FALSE);

  m_shaders["Grid"]->use();
  m_shaders["Grid"]->setMat4("view", view);
  m_shaders["Grid"]->setMat4("projection", projection);

  glBindVertexArray(gridVAO);
  glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
  glBindVertexArray(0);

  glDepthMask(GL_TRUE);
  glDisable(GL_BLEND);
}

// ---------------------------------------------------------------------------
//  renderLightObject()  --  Draw the visual sun/light cube
// ---------------------------------------------------------------------------

void RenderEngine::renderLightObject(const glm::mat4 &view,
                                     const glm::mat4 &projection) {
  m_shaders["Light"]->use();
  glm::mat4 model = glm::mat4(1.0f);
  model = glm::translate(model, m_lightPos);
  m_shaders["Light"]->setMat4("model", model);
  m_shaders["Light"]->setMat4("view", view);
  m_shaders["Light"]->setMat4("projection", projection);

  glBindVertexArray(lightVAO);
  glDrawArrays(GL_TRIANGLES, 0, 36);
  glBindVertexArray(0);
}

// ---------------------------------------------------------------------------
//  runRenderLoop()  --  Main application loop
// ---------------------------------------------------------------------------

void RenderEngine::runRenderLoop() {
  GLFWwindow *window = m_windowContext->getWindow();
  bool modelLoaded = false;
  glEnable(GL_DEPTH_TEST);

  const char *shaderNames[] = {"PBR (Simplified)", "Blinn-Phong", "Holographic",
                               "Normals Debug", "Toon Shading"};
  int currentShaderIdx = 0;
  m_activeShaderName = shaderNames[0]; // Default to PBR for glTF materials

  float menuBarHeight = 0.0f;

  // ---- Main Loop --------------------------------------------------------
  while (!m_windowContext->shouldClose()) {
    m_windowContext->pollEvents();

    // 1. Poll UDP data and execute Gesture State Machine via Controller
    DataPacket packet = m_udpReceiver->pollData();
    m_gestureController->processGestures(packet, *m_camera);

    // 2. Start ImGui Frame First (so we get accurate IO state like MouseWheel)
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    ImGuiIO &io = ImGui::GetIO();

    // 3. Camera & Input Handling (Maya/Blender style Mouse fallback)

    // --- Mouse Controls ---
    if (!io.WantCaptureMouse) {
      double mx, my;
      glfwGetCursorPos(window, &mx, &my);

      if (m_firstMouse) {
        m_lastMouseX = (float)mx;
        m_lastMouseY = (float)my;
        m_firstMouse = false;
      }

      float dx = (float)mx - m_lastMouseX;
      float dy = (float)my - m_lastMouseY;
      m_lastMouseX = (float)mx;
      m_lastMouseY = (float)my;

      bool middleDown =
          glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS;
      bool shiftDown = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
                       glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;

      // Orbit: Middle drag (without shift)
      if (middleDown && !shiftDown) {
        m_camera->orbit(dx * 0.3f, dy * 0.3f);
      }

      // Pan: Shift + Middle drag
      if (middleDown && shiftDown) {
        m_camera->pan(-dx, dy);
      }

      // Zoom: Scroll wheel
      if (io.MouseWheel != 0.0f) {
        m_camera->zoom(-io.MouseWheel * 1.5f);
      }
    } else {
      m_firstMouse = true;
    }

    // --- Keyboard Controls (WASD) ---
    if (!io.WantTextInput) {
      float wasdSpeed = 0.05f * m_camera->getZoom();

      if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        m_camera->pan(0.0f, wasdSpeed * 100.0f);
      if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        m_camera->pan(0.0f, -wasdSpeed * 100.0f);
      if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        m_camera->pan(wasdSpeed * 100.0f, 0.0f);
      if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        m_camera->pan(-wasdSpeed * 100.0f, 0.0f);
    }

    // 4. Viewport & Clear Screen
    int fbW = m_windowContext->getWidth();
    int fbH = m_windowContext->getHeight();
    glViewport(0, 0, fbW, fbH);

    glClearColor(0.08f, 0.08f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // 5. Matrices
    float aspect = (fbH > 0) ? (float)fbW / (float)fbH : 1.0f;

    glm::mat4 projection =
        glm::perspective(glm::radians(45.0f), aspect, 0.1f, 1000.0f);
    glm::mat4 view = m_camera->getViewMatrix();
    glm::vec3 eye = m_camera->getPosition();

    // 5. Render infinite grid
    renderGrid(view, projection);

    // 6. Render the Light Object
    renderLightObject(view, projection);

    // 7. Render Model with glTF / OBJ multi-primitives and materials
    if (m_spatialOverlay->isLoaded()) {
      ShaderManager *activeShader = m_shaders[m_activeShaderName].get();

      activeShader->use();
      activeShader->setMat4("view", view);
      activeShader->setMat4("projection", projection);
      activeShader->setVec3("cameraPos", eye);
      activeShader->setVec3("lightPos", m_lightPos);
      activeShader->applyHolographicFade(1.0f);

      m_spatialOverlay->draw(activeShader);
      modelLoaded = true;
    } else {
      modelLoaded = false;
    }

    // 8. ImGui UI
    if (ImGui::BeginMainMenuBar()) {
      menuBarHeight = ImGui::GetWindowSize().y;

      if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("Open 3D Model (.gltf, .glb, .obj)...", "Ctrl+O")) {
          std::string path = openFileDialog();
          if (!path.empty()) {
            std::cout << "[RenderEngine] Loading model: " << path << std::endl;
            modelLoaded = m_spatialOverlay->loadModel(path);
          }
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Exit", "Alt+F4")) {
          glfwSetWindowShouldClose(window, GLFW_TRUE);
        }
        ImGui::EndMenu();
      }

      if (ImGui::BeginMenu("View")) {
        if (ImGui::MenuItem("Reset Camera")) {
          m_camera->reset();
        }
        ImGui::MenuItem("Scene Controls", NULL, &m_showSceneControls);
        ImGui::EndMenu();
      }

      ImGui::SetCursorPosX(ImGui::GetContentRegionMax().x - 450.0f);
      if (modelLoaded) {
        ImGui::TextDisabled("Model loaded  |  MMB: Orbit  |  Shift+MMB / WASD: Pan  |  Scroll: Zoom");
      } else {
        ImGui::TextDisabled("No model loaded  |  File > Open 3D Model (.gltf, .glb, .obj)...");
      }
      ImGui::EndMainMenuBar();
    }

    // Fixed Side Panel for Scene Controls
    if (m_showSceneControls) {
      float panelWidth = 320.0f;
      ImGui::SetNextWindowPos(ImVec2((float)fbW - panelWidth, menuBarHeight),
                              ImGuiCond_Always);
      ImGui::SetNextWindowSize(ImVec2(panelWidth, (float)fbH - menuBarHeight),
                               ImGuiCond_Always);

      ImGuiWindowFlags panelFlags =
          ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
          ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar;

      ImGui::Begin("Scene Controls Panel", &m_showSceneControls, panelFlags);

      ImGui::TextDisabled("--- RENDERING ---");
      ImGui::Spacing();
      ImGui::Text("Active Shader");
      ImGui::SetNextItemWidth(-1);
      if (ImGui::Combo("##shader_combo", &currentShaderIdx, shaderNames,
                       IM_ARRAYSIZE(shaderNames))) {
        m_activeShaderName = shaderNames[currentShaderIdx];
      }

      ImGui::Spacing();
      ImGui::Separator();
      ImGui::Spacing();

      ImGui::TextDisabled("--- LIGHTING ---");
      ImGui::Spacing();
      ImGui::Text("Light Position (X,Y,Z)");
      ImGui::SetNextItemWidth(-1);
      ImGui::SliderFloat3("##lightpos", &m_lightPos.x, -20.0f, 20.0f);

      ImGui::Spacing();
      ImGui::Separator();
      ImGui::Spacing();

      ImGui::TextDisabled("--- MODEL INSPECTOR ---");
      ImGui::Spacing();
      if (m_spatialOverlay->isLoaded()) {
        std::string path = m_spatialOverlay->getLoadedFilePath();
        size_t lastSlash = path.find_last_of("/\\");
        std::string filename = (lastSlash != std::string::npos) ? path.substr(lastSlash + 1) : path;

        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "File: %s", filename.c_str());
        ImGui::Text("Primitives: %zu", m_spatialOverlay->getPrimitiveCount());
        ImGui::Text("Vertices:   %zu", m_spatialOverlay->getVertexCount());
        ImGui::Text("Triangles:  %zu", m_spatialOverlay->getTriangleCount());
        ImGui::Text("Textures:   %zu", m_spatialOverlay->getTextureCount());
        ImGui::Text("Materials:  %zu", m_spatialOverlay->getMaterialCount());
      } else {
        ImGui::TextDisabled("No model loaded");
      }

      ImGui::Spacing();
      ImGui::Separator();
      ImGui::Spacing();

      ImGui::TextDisabled("--- AI GESTURE TELEMETRY ---");
      ImGui::Spacing();
      int handCount = m_gestureController->getDetectedHandCount();
      if (handCount > 0) {
        ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.4f, 1.0f), "Tracking: Active (%d Hand%s)", handCount, handCount > 1 ? "s" : "");
        ImGui::Text("Hand 1: %s", m_gestureController->getPrimaryGesture().c_str());
        if (handCount >= 2) {
          ImGui::Text("Hand 2: %s", m_gestureController->getSecondaryGesture().c_str());
        }
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Action: %s", m_gestureController->getCurrentAction().c_str());
      } else {
        ImGui::TextDisabled("Tracking: Idle (No hands in FOV)");
      }

      ImGui::Spacing();
      ImGui::Separator();
      ImGui::Spacing();

      ImGui::TextDisabled("--- CAMERA INFO ---");
      ImGui::Spacing();
      glm::vec3 camTarget = m_camera->getTarget();
      ImGui::Text("Target: (%.1f, %.1f, %.1f)", camTarget.x, camTarget.y,
                  camTarget.z);
      ImGui::Text("Zoom: %.1f", m_camera->getZoom());

      ImGui::End();
    }

    // Keyboard shortcut: Ctrl+O
    if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_O)) {
      std::string path = openFileDialog();
      if (!path.empty()) {
        modelLoaded = m_spatialOverlay->loadModel(path);
      }
    }

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    // 9. Swap
    m_windowContext->swapBuffers();
  }
}
