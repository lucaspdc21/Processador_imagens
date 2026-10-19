#pragma once
#include "AppState.hpp"

namespace ui {

    // Janela "Escala e rotacao": escala (vizinho / bilinear) e rotacao com
    // pivo (vizinho / bilinear), com previa lado a lado (entrada | resultado).
    void RenderSpatialWindow(AppState& state);

}
