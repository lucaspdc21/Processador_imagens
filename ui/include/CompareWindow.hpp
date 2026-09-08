#pragma once
#include "AppState.hpp"

namespace ui {

    // Aplica cada transformacao de intensidade Chamada automaticamente por RenderCompareWindow
    // na primeira vez que a janela abre, e tambem pelo botao "Atualizar
    // comparação" dentro dela.
    void BuildCompareTextures(AppState& state);
    void RenderCompareWindow(AppState& state);

}
