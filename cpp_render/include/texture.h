#pragma once

#include <glad/glad.h>
#include <string>
#include <vector>

/**
 * @class Texture
 * @brief Manages an OpenGL 2D texture, loading from memory buffer or file path,
 *        with automatic mipmapping and sampler configuration.
 */
class Texture {
public:
    Texture();
    ~Texture();

    // Disable copy to avoid accidental double-deletion of GPU texture IDs
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    // Allow move semantics
    Texture(Texture&& other) noexcept;
    Texture& operator=(Texture&& other) noexcept;

    /**
     * @brief Load texture from raw memory (e.g., glTF embedded buffer / image data).
     * @param data Pointer to raw image file bytes (PNG, JPEG) or decoded pixels.
     * @param size Size in bytes of the raw data buffer (if uncompressed / raw pixel pointer, see width/height).
     * @param isSRGB True if this texture contains color data (BaseColor/Albedo), false for data maps (Normal, Roughness).
     * @param wrapS GL wrap mode for S coordinate (e.g., GL_REPEAT).
     * @param wrapT GL wrap mode for T coordinate (e.g., GL_REPEAT).
     * @param minFilter GL minification filter (e.g., GL_LINEAR_MIPMAP_LINEAR).
     * @param magFilter GL magnification filter (e.g., GL_LINEAR).
     */
    bool loadFromMemory(const unsigned char* data, int size, bool isSRGB = true,
                        GLint wrapS = GL_REPEAT, GLint wrapT = GL_REPEAT,
                        GLint minFilter = GL_LINEAR_MIPMAP_LINEAR, GLint magFilter = GL_LINEAR);

    /**
     * @brief Load texture directly from already decoded pixel buffer (e.g. tinygltf parsed image).
     */
    bool loadFromDecodedPixels(const unsigned char* pixels, int width, int height, int channels,
                               bool isSRGB = true,
                               GLint wrapS = GL_REPEAT, GLint wrapT = GL_REPEAT,
                               GLint minFilter = GL_LINEAR_MIPMAP_LINEAR, GLint magFilter = GL_LINEAR);

    /**
     * @brief Load texture from an external image file on disk.
     */
    bool loadFromFile(const std::string& filePath, bool isSRGB = true,
                      GLint wrapS = GL_REPEAT, GLint wrapT = GL_REPEAT,
                      GLint minFilter = GL_LINEAR_MIPMAP_LINEAR, GLint magFilter = GL_LINEAR);

    /** Bind this texture to the specified OpenGL texture unit (0..N). */
    void bind(GLuint unit = 0) const;

    /** Unbind texture from the specified texture unit. */
    void unbind(GLuint unit = 0) const;

    GLuint getID() const { return m_id; }
    int getWidth() const { return m_width; }
    int getHeight() const { return m_height; }
    int getChannels() const { return m_channels; }
    bool isValid() const { return m_id != 0; }

private:
    GLuint m_id;
    int m_width;
    int m_height;
    int m_channels;

    void cleanup();
};
