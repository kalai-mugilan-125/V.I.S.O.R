#include "texture.h"
#include <iostream>

#include <stb_image.h>

Texture::Texture()
    : m_id(0), m_width(0), m_height(0), m_channels(0) {}

Texture::~Texture() {
    cleanup();
}

Texture::Texture(Texture&& other) noexcept
    : m_id(other.m_id), m_width(other.m_width), m_height(other.m_height), m_channels(other.m_channels) {
    other.m_id = 0;
    other.m_width = 0;
    other.m_height = 0;
    other.m_channels = 0;
}

Texture& Texture::operator=(Texture&& other) noexcept {
    if (this != &other) {
        cleanup();
        m_id = other.m_id;
        m_width = other.m_width;
        m_height = other.m_height;
        m_channels = other.m_channels;
        other.m_id = 0;
        other.m_width = 0;
        other.m_height = 0;
        other.m_channels = 0;
    }
    return *this;
}

void Texture::cleanup() {
    if (m_id != 0) {
        glDeleteTextures(1, &m_id);
        m_id = 0;
    }
    m_width = 0;
    m_height = 0;
    m_channels = 0;
}

bool Texture::loadFromDecodedPixels(const unsigned char* pixels, int width, int height, int channels,
                                   bool isSRGB, GLint wrapS, GLint wrapT,
                                   GLint minFilter, GLint magFilter) {
    if (!pixels || width <= 0 || height <= 0) {
        std::cerr << "[Texture] Invalid decoded pixel data." << std::endl;
        return false;
    }

    cleanup();
    m_width = width;
    m_height = height;
    m_channels = channels;

    GLenum internalFormat = GL_RGBA;
    GLenum format = GL_RGBA;

    if (channels == 1) {
        internalFormat = GL_RED;
        format = GL_RED;
    } else if (channels == 2) {
        internalFormat = GL_RG;
        format = GL_RG;
    } else if (channels == 3) {
        internalFormat = isSRGB ? GL_SRGB8 : GL_RGB8;
        format = GL_RGB;
    } else if (channels == 4) {
        internalFormat = isSRGB ? GL_SRGB8_ALPHA8 : GL_RGBA8;
        format = GL_RGBA;
    }

    glGenTextures(1, &m_id);
    glBindTexture(GL_TEXTURE_2D, m_id);

    // Pixel storage alignment for non-multiple of 4 widths
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, format, GL_UNSIGNED_BYTE, pixels);
    glGenerateMipmap(GL_TEXTURE_2D);

    // Apply sampler parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrapS);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minFilter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, magFilter);

    glBindTexture(GL_TEXTURE_2D, 0);
    return true;
}

bool Texture::loadFromMemory(const unsigned char* data, int size, bool isSRGB,
                             GLint wrapS, GLint wrapT,
                             GLint minFilter, GLint magFilter) {
    if (!data || size <= 0) return false;

    int w, h, comp;
    unsigned char* decoded = stbi_load_from_memory(data, size, &w, &h, &comp, 4);
    if (!decoded) {
        std::cerr << "[Texture] Failed to decode image from memory: " << stbi_failure_reason() << std::endl;
        return false;
    }

    bool success = loadFromDecodedPixels(decoded, w, h, 4, isSRGB, wrapS, wrapT, minFilter, magFilter);
    stbi_image_free(decoded);
    return success;
}

bool Texture::loadFromFile(const std::string& filePath, bool isSRGB,
                           GLint wrapS, GLint wrapT,
                           GLint minFilter, GLint magFilter) {
    int w, h, comp;
    // Do not flip vertically for glTF standard (glTF UV coordinates origin is top-left)
    stbi_set_flip_vertically_on_load(false);
    unsigned char* decoded = stbi_load(filePath.c_str(), &w, &h, &comp, 4);
    if (!decoded) {
        std::cerr << "[Texture] Failed to load image from file: " << filePath << " (" << stbi_failure_reason() << ")" << std::endl;
        return false;
    }

    bool success = loadFromDecodedPixels(decoded, w, h, 4, isSRGB, wrapS, wrapT, minFilter, magFilter);
    stbi_image_free(decoded);
    return success;
}

void Texture::bind(GLuint unit) const {
    if (m_id != 0) {
        glActiveTexture(GL_TEXTURE0 + unit);
        glBindTexture(GL_TEXTURE_2D, m_id);
    }
}

void Texture::unbind(GLuint unit) const {
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, 0);
}
