#pragma once

namespace ui {

    // Grafico interativo da transformacao linear definida por partes (alargamento
    // de contraste): T(r) com r (entrada) no eixo x e s (saida) no eixo y, ambos
    // em [0, 255]. A curva passa por (0,0), (r1,s1), (r2,s2) e (255,255).
    //
    // Os pontos (r1,s1) e (r2,s2) podem ser arrastados com o mouse; o grafico e a
    // curva sao redesenhados a cada quadro. Mantem r1 <= r2.
    // 'side' e o lado do grafico quadrado em pixels (<= 0: usa a largura disponivel).
    // Devolve true se algum valor mudou neste quadro.
    bool ContrastCurveEditor(const char* id, int& r1, int& s1, int& r2, int& s2, float side = 0.0f);

}
