// ============================================================================
//  main.cpp  --  V.I.S.O.R. Application Entry Point
//  Creates the RenderEngine and starts the application.
// ============================================================================

#include "render_engine.h"
#include <iostream>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <direct.h>
#endif

/**
 * @brief Sets the working directory to the directory that contains this
 *        executable, so that relative asset paths (shaders/, models/) always
 *        resolve correctly regardless of where the process was launched from.
 */
static void setWorkDirToExeDir() {
#ifdef _WIN32
    char exePath[MAX_PATH] = { 0 };
    GetModuleFileNameA(NULL, exePath, MAX_PATH);

    // Strip the filename to get just the directory
    char* lastSlash = strrchr(exePath, '\\');
    if (lastSlash) {
        *lastSlash = '\0';
        _chdir(exePath);
        std::cout << "[Main] Working directory set to: " << exePath << std::endl;
    }
#endif
}

int main() {
    // Fix shader/asset resolution for packaged standalone deployment
    setWorkDirToExeDir();

    std::cout << "============================================" << std::endl;
    std::cout << " V.I.S.O.R. - Virtual Interface Spatial    " << std::endl;
    std::cout << "          Overlay Renderer                  " << std::endl;
    std::cout << "============================================" << std::endl;

    // Create the top-level render engine (owns all subsystems)
    RenderEngine engine;

    // Initialize all subsystems: window, shaders, grid, UDP, ImGui
    if (!engine.initialize()) {
        std::cerr << "[FATAL] Engine initialization failed. Exiting." << std::endl;
        return -1;
    }

    // Enter the main render loop (blocks until the window is closed)
    engine.runRenderLoop();

    std::cout << "[Main] Application exited cleanly." << std::endl;
    return 0;
}
