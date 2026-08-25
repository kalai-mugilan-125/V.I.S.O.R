cmake_minimum_required(VERSION 3.14)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "" FORCE)
project(VISOR_Cpp_Render_Engine CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# --- GLFW ---
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.3.8
)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

# --- GLM ---
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        master
)
FetchContent_MakeAvailable(glm)

# -----------------------------------------------------------------------------
# nlohmann_json (Header only JSON library)
# -----------------------------------------------------------------------------
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.2
)
FetchContent_MakeAvailable(nlohmann_json)

# --- TinyObjLoader ---
FetchContent_Declare(
    tinyobjloader
    GIT_REPOSITORY https://github.com/tinyobjloader/tinyobjloader.git
    GIT_TAG        release
)
FetchContent_MakeAvailable(tinyobjloader)

# --- TinyGLTF ---
FetchContent_Declare(
    tinygltf
    GIT_REPOSITORY https://github.com/syoyo/tinygltf.git
    GIT_TAG        v2.9.3
)
set(TINYGLTF_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(tinygltf)

# --- ImGui ---
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        master
)
FetchContent_MakeAvailable(imgui)

# --- GLAD (Pre-generated for OpenGL 3.3 Core) ---
FetchContent_Declare(
    glad
    GIT_REPOSITORY https://github.com/libigl/libigl-glad.git
    GIT_TAG        master
)
FetchContent_MakeAvailable(glad)

# Create an ImGui library target
add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)
target_link_libraries(imgui PRIVATE glfw glad)

# Main executable
add_executable(visor
    src/main.cpp
    src/render_engine.cpp
    src/window_context.cpp
    src/shader_manager.cpp
    src/mesh.cpp
    src/model.cpp
    src/model_loader.cpp
    src/spatial_overlay.cpp
    src/texture.cpp
    src/udp_server.cpp
    src/camera.cpp
    src/gesture_controller.cpp
)

target_include_directories(visor PRIVATE
    include
    ${glad_SOURCE_DIR}/include
    ${glfw_SOURCE_DIR}/include
    ${glm_SOURCE_DIR}
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
    ${tinyobjloader_SOURCE_DIR}
    ${tinygltf_SOURCE_DIR}
)

target_compile_definitions(visor PRIVATE GLM_ENABLE_EXPERIMENTAL)

# Link Windows Sockets (ws2_32) and native file dialog (comdlg32) on Windows
if(WIN32)
    target_link_libraries(visor PRIVATE ws2_32 comdlg32)
endif()

# OpenGL on Windows
find_package(OpenGL REQUIRED)

target_link_libraries(visor PRIVATE
    glfw
    glm::glm
    tinyobjloader
    imgui
    glad
    OpenGL::GL
    nlohmann_json::nlohmann_json
)

# Copy shaders to build directory
file(COPY shaders DESTINATION ${CMAKE_BINARY_DIR})
