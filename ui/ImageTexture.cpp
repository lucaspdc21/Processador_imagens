#include "ImageTexture.hpp"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>
#include <vector>

namespace ui {

unsigned int UploadImageToTexture(const core::Image& image, unsigned int existingTextureId, bool normalize) {
    int rows = image.getRows();
    int cols = image.getCols();
    int channels = image.getChannels();
    if (rows <= 0 || cols <= 0) return existingTextureId;

    GLuint texture = static_cast<GLuint>(existingTextureId);
    if (texture == 0) {
        glGenTextures(1, &texture);
    }

    // Normalizacao min-max so para exibicao (resultados de filtros podem ter
    // valores negativos ou acima de 255).
    const std::vector<float>& samples = image.samples();
    float lo = 0.0f;
    float scale = 1.0f;
    if (normalize && !samples.empty()) {
        auto range = std::minmax_element(samples.begin(), samples.end());
        lo = *range.first;
        float hi = *range.second;
        scale = (hi > lo) ? 255.0f / (hi - lo) : 0.0f; // imagem constante vira preta
    }

    auto toByte = [&](float v) -> unsigned char {
        if (normalize) v = (v - lo) * scale;
        long r = std::lround(v);
        return static_cast<unsigned char>(std::max(0L, std::min(r, 255L)));
    };

    std::vector<unsigned char> rgba(static_cast<size_t>(rows) * cols * 4);
    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < cols; ++x) {
            size_t idx = (static_cast<size_t>(y) * cols + x) * 4;
            // 1 canal: cinza replicado em R, G, B. 3 canais: RGB.
            rgba[idx + 0] = toByte(image.at(y, x, 0));
            rgba[idx + 1] = toByte(image.at(y, x, channels == 3 ? 1 : 0));
            rgba[idx + 2] = toByte(image.at(y, x, channels == 3 ? 2 : 0));
            rgba[idx + 3] = 255;
        }
    }

    glBindTexture(GL_TEXTURE_2D, texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, cols, rows, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());

    return static_cast<unsigned int>(texture);
}

void RefreshTexture(AppState& state) {
    if (!state.hasImage) return;
    state.textureId = UploadImageToTexture(state.workingImage, state.textureId, state.displayNormalized);
}

}
