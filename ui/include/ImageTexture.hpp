#pragma once
#include "AppState.hpp"

namespace ui {

    void RefreshTexture(AppState& state);

    // Envia a imagem (1 ou 3 canais) para uma textura OpenGL e devolve o id.
    // normalize = true aplica normalizacao min-max (v' = (v - min) * 255 / (max - min))
    // apenas no buffer da textura; os dados da imagem nao sao alterados.
    unsigned int UploadImageToTexture(const core::Image& image, unsigned int existingTextureId = 0,
                                      bool normalize = false);

}
