#include "ImageTexture.hpp"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <vector>

namespace ui {

unsigned int UploadImageToTexture(core::Image& image, unsigned int existingTextureId) {
    int rows = image.getRows();
    int cols = image.getCols();
    if (rows <= 0 || cols <= 0) return existingTextureId;

    GLuint texture = static_cast<GLuint>(existingTextureId);
    if (texture == 0) {
        glGenTextures(1, &texture);
    }

    std::vector<unsigned char> rgba(static_cast<size_t>(rows) * cols * 4);
    const auto& data = image.getData();
    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < cols; ++x) {
            int v = std::max(0, std::min(data[y][x], 255));
            size_t idx = (static_cast<size_t>(y) * cols + x) * 4;
            rgba[idx + 0] = static_cast<unsigned char>(v);
            rgba[idx + 1] = static_cast<unsigned char>(v);
            rgba[idx + 2] = static_cast<unsigned char>(v);
            rgba[idx + 3] = 255;
        }
    }

    glBindTexture(GL_TEXTURE_2D, texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, cols, rows, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());

    return static_cast<unsigned int>(texture);
}

void RefreshTexture(AppState& state) {
    if (!state.hasImage) return;
    state.textureId = UploadImageToTexture(state.workingImage, state.textureId);
}

}
