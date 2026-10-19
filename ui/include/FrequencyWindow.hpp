#pragma once
#include "AppState.hpp"

namespace ui {

    // Janela "Filtragem no dominio da frequencia": DFT/FFT, espectro
    // centralizado editavel (pincel, filtros pre-definidos) e transformada
    // inversa que gera a imagem filtrada.
    void RenderFrequencyWindow(AppState& state);

}
