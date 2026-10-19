#pragma once
#include <vector>

namespace core {

    // Filtros pre-definidos no dominio da frequencia (Secao 4.10 do plano).
    // Geram a mascara real H(u, v) de tamanho P x Q (linha a linha), valores
    // em [0, 1], para o espectro CENTRALIZADO: a origem fica em (P/2, Q/2).
    // D(u, v) = sqrt((u - P/2)^2 + (v - Q/2)^2), u = linha, v = coluna.

    enum class FilterKind {
        LowPass,        // passa-baixa
        HighPass,       // passa-alta:    H_HP = 1 - H_LP            (Eq. 4.9-1)
        BandReject,     // rejeita-banda
        BandPass,       // passa-banda:   H_BP = 1 - H_BR            (Eq. 4.10-1)
        NotchReject,    // notch rejeita (pares simetricos)
        NotchPass       // notch passa:   H_PN = 1 - H_NR            (Eq. 4.10-6)
    };

    enum class FilterShape { Ideal, Butterworth, Gaussian };

    // Um notch (par simetrico) centrado em (+u_k, +v_k) e (-u_k, -v_k)
    // RELATIVOS ao centro do retangulo de frequencia (P/2, Q/2).
    struct Notch {
        double u = 0.0;     // deslocamento em linhas
        double v = 0.0;     // deslocamento em colunas
        double d0 = 10.0;   // raio D_0k
    };

    struct FilterParams {
        FilterKind kind = FilterKind::LowPass;
        FilterShape shape = FilterShape::Gaussian;
        double d0 = 30.0;          // frequencia de corte (pixels do espectro)
        int order = 2;             // ordem n do Butterworth
        double width = 10.0;       // largura W da banda
        std::vector<Notch> notches;
    };

    const char* filterKindName(FilterKind kind);
    const char* filterShapeName(FilterShape shape);

    // Gera H (P*Q valores). Se 'params' for invalido (P,Q <= 0), retorna vazio.
    std::vector<float> makeFilter(int P, int Q, const FilterParams& params);

    // Atalho: um unico par de notches (rejeita), usado pela ferramenta
    // "notch por clique". (u, v) sao as coordenadas ABSOLUTAS (linha, coluna)
    // do ponto clicado no espectro centralizado; o par simetrico e incluido.
    std::vector<float> makeNotchRejectAt(int P, int Q, double row, double col,
                                         double d0, int order, FilterShape shape);

}
