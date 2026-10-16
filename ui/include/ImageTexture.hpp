#pragma once
#include "AppState.hpp"

namespace ui {

    void RefreshTexture(AppState& state);
    // forceNormalize: normaliza (min-max) a imagem para exibicao mesmo que os
    // valores estejam em [0, 255] (resultado de Laplaciano, Sobel, gradiente).
    unsigned int UploadImageToTexture(core::Image& image, unsigned int existingTextureId = 0, bool forceNormalize = false);

}
