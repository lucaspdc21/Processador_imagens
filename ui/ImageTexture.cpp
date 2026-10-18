#include "ImageTexture.hpp"
#include "ImageConvolution.hpp"
#include "ImageStats.hpp"
#include "ImageColor.hpp"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <vector>

namespace ui {

unsigned int UploadImageToTexture(core::ColorImage& image, unsigned int existingTextureId, bool forceNormalize) {
    int rows = image.getRows();
    int cols = image.getCols();
    if (rows <= 0 || cols <= 0) return existingTextureId;

    GLuint texture = static_cast<GLuint>(existingTextureId);
    if (texture == 0) {
        glGenTextures(1, &texture);
    }

    // Filtros com resultado nao limitado geram valores negativos ou acima de 255.
    // Nesse caso a imagem e normalizada (min-max) apenas para exibicao; a
    // imagem de trabalho nao e alterada.
    core::ImageStats stats = core::computeStats(image);
    core::ColorImage normalized;
    core::ColorImage* shown = &image;
    if (forceNormalize || stats.minValue < 0 || stats.maxValue > 255) {
        normalized = core::normalizeForDisplay(image);
        shown = &normalized;
    }

    std::vector<unsigned char> rgba(static_cast<size_t>(rows) * cols * 4);
    const auto& dataR = shown->r().getData();
    const auto& dataG = shown->g().getData();
    const auto& dataB = shown->b().getData();
    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < cols; ++x) {
            size_t idx = (static_cast<size_t>(y) * cols + x) * 4;
            rgba[idx + 0] = static_cast<unsigned char>(std::max(0, std::min(dataR[y][x], 255)));
            rgba[idx + 1] = static_cast<unsigned char>(std::max(0, std::min(dataG[y][x], 255)));
            rgba[idx + 2] = static_cast<unsigned char>(std::max(0, std::min(dataB[y][x], 255)));
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
    // Visualizacao de um plano (R, G, B, H, S ou I): so o que e exibido muda
    if (state.colorView() != core::ColorView::Color) {
        core::ColorImage view = core::makeView(state.workingImage, state.colorView());
        state.textureId = UploadImageToTexture(view, state.textureId, state.normalizeNextDisplay);
    } else {
        state.textureId = UploadImageToTexture(state.workingImage, state.textureId, state.normalizeNextDisplay);
    }
    state.normalizeNextDisplay = false;
    state.histogramDirty = true;
}

}
