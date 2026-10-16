#pragma once
#include "AppState.hpp"

namespace ui {

    // Janela com o editor do kernel (tamanho e coeficientes) e as opcoes do
    // filtro por convolucao. Aberta por state.showConvolutionWindow.
    void RenderConvolutionWindow(AppState& state);

}
