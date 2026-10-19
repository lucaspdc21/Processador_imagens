#pragma once
#include "FrequencyMask.hpp"
#include <cstdint>

namespace ui {

    // Envia uma imagem RGBA 8 bits (rows x cols, linha a linha) para uma
    // textura OpenGL (cria se existingTextureId == 0). Exibicao sem suavizar
    // (vizinho mais proximo), para ver os pixels do espectro.
    unsigned int UploadRgbaFull(unsigned int existingTextureId, const uint8_t* rgba, int rows, int cols);

    // Atualiza so um retangulo da textura (glTexSubImage2D): usado ao pintar
    // o espectro, para nao reenviar a imagem inteira a cada pincelada.
    void UploadRgbaRect(unsigned int textureId, const uint8_t* rgba, int totalCols, const core::Rect& rect);

}
