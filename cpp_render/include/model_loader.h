#pragma once

#include <string>

// Forward declaration
class Model;

/**
 * @class ModelLoader
 * @brief Factory loader that parses 3D model formats (glTF 2.0 .gltf/.glb and Wavefront .obj)
 *        and populates a Model entity.
 */
class ModelLoader {
public:
    /**
     * @brief Loads a 3D model from disk into the specified Model container.
     * @param filePath Path to the 3D model file.
     * @param outModel Target Model object to populate.
     * @return true if loading succeeded, false otherwise.
     */
    static bool load(const std::string& filePath, Model& outModel);

private:
    static bool loadGLTF(const std::string& filePath, Model& outModel);
    static bool loadOBJ(const std::string& filePath, Model& outModel);
};
