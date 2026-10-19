#include "RgbaTexture.hpp"
#include <GLFW/glfw3.h>

namespace ui {

unsigned int UploadRgbaFull(unsigned int existingTextureId, const uint8_t* rgba, int rows, int cols) {
    if (rows <= 0 || cols <= 0 || rgba == nullptr) return existingTextureId;

    GLuint texture = static_cast<GLuint>(existingTextureId);
    if (texture == 0) glGenTextures(1, &texture);

    glBindTexture(GL_TEXTURE_2D, texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, cols, rows, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    return static_cast<unsigned int>(texture);
}

void UploadRgbaRect(unsigned int textureId, const uint8_t* rgba, int totalCols, const core::Rect& rect) {
    if (textureId == 0 || rgba == nullptr || rect.empty()) return;
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(textureId));
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, totalCols);     // pula o resto de cada linha da imagem grande
    glTexSubImage2D(GL_TEXTURE_2D, 0, rect.c0, rect.r0, rect.cols(), rect.rows(),
                    GL_RGBA, GL_UNSIGNED_BYTE,
                    rgba + (static_cast<size_t>(rect.r0) * totalCols + rect.c0) * 4);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
}

}
