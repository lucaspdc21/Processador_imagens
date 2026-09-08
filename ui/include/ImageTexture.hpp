#pragma once
#include "AppState.hpp"

namespace ui {

    void RefreshTexture(AppState& state);
    unsigned int UploadImageToTexture(core::Image& image, unsigned int existingTextureId = 0);

}
