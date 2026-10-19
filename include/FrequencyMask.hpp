#pragma once
#include <vector>

namespace core {

    // Retangulo de pixels [r0, r1) x [c0, c1) (r1 e c1 exclusivos).
    struct Rect {
        int r0 = 0, c0 = 0, r1 = 0, c1 = 0;

        bool empty() const { return r1 <= r0 || c1 <= c0; }
        int rows() const { return r1 - r0; }
        int cols() const { return c1 - c0; }
        void unite(const Rect& o) {
            if (o.empty()) return;
            if (empty()) { *this = o; return; }
            if (o.r0 < r0) r0 = o.r0;
            if (o.c0 < c0) c0 = o.c0;
            if (o.r1 > r1) r1 = o.r1;
            if (o.c1 > c1) c1 = o.c1;
        }
    };

    // Pincel do espectro (Secao 4.9). Trabalha sobre a mascara H.
    //   valor v_pincel = 0 (preto) | 1 (branco) | tom/255 (cinza)
    //   H <- H * (1 - a*opacidade) + v_pincel * a * opacidade
    struct Brush {
        double radius = 12.0;       // raio (pixels do espectro)
        double hardness = 0.5;      // 0..1 (so no pincel suave)
        double opacity = 1.0;       // 0..1
        double value = 0.0;         // 0 = preto (remove) .. 1 = branco (restaura)
        bool hardDisc = false;      // true: disco duro (alpha = 1 se d <= raio)
    };

    // Mascara multiplicativa real H(u, v) em [0, 1] (P x Q, linha a linha),
    // inicialmente 1 em tudo. Guarda historico de desfazer/refazer por
    // REGIAO alterada (uma pincelada grande nao copia a mascara inteira).
    //
    // Simetria conjugada (obrigatoria para imagem real): cada carimbo em
    // (u, v) tambem e aplicado no ponto espelhado ((P-u) mod P, (Q-v) mod Q),
    // combinando os dois alfas ANTES de aplicar: a = 1 - (1 - a1)(1 - a2).
    // Assim H fica EXATAMENTE simetrica: H[u][v] == H[(P-u)%P][(Q-v)%Q].
    class SpectrumMask {
    public:
        void reset(int P, int Q);               // tudo 1, apaga o historico
        int rows() const { return rows_; }
        int cols() const { return cols_; }
        bool valid() const { return rows_ > 0 && cols_ > 0; }

        const std::vector<float>& values() const { return h_; }
        float at(int r, int c) const { return h_[static_cast<size_t>(r) * cols_ + c]; }

        // Pincelada = beginStroke(); varias paintSegment(); endStroke().
        void beginStroke();
        // Carimba ao longo do segmento (r0,c0)->(r1,c1) (linha, coluna, reais).
        // Retorna ate 2 retangulos alterados (regiao do pincel e a espelhada).
        std::vector<Rect> paintSegment(double r0, double c0, double r1, double c1, const Brush& brush);
        // Fecha a pincelada e registra o desfazer. Retorna a regiao alterada.
        Rect endStroke();

        // Operacoes na mascara inteira (cada uma e uma entrada do historico).
        Rect multiplyBy(const std::vector<float>& filter);   // H <- H * filtro (combina filtros)
        Rect invert();                                         // H <- 1 - H
        Rect clear();                                          // H <- 1

        bool canUndo() const { return !undo_.empty(); }
        bool canRedo() const { return !redo_.empty(); }
        Rect undo();
        Rect redo();

        // Perfil do pincel: alfa em funcao da distancia d ao centro.
        // Suave: sigma = raio*(1 - 0,9*dureza), a = exp(-t^2/(2 sigma^2)) com
        // t = max(0, d - nucleo) e nucleo = max(0, raio - 3 sigma) (nucleo plano
        // so aparece com dureza alta: dureza 1 vira quase um disco). Para
        // nucleo = 0 e exatamente a formula do plano. Duro: disco.
        static double brushAlpha(double d, const Brush& brush);
        // Raio (inteiro) da janela de pixels afetada pelo pincel.
        static int brushExtent(const Brush& brush);

    private:
        struct Entry {
            Rect rect;
            std::vector<float> before;
            std::vector<float> after;
        };

        void stamp(double rc, double cc, const Brush& brush, Rect& winRect, Rect& mirRect);
        void commit(const Rect& bbox);
        void trimHistory();
        static void copyRect(const std::vector<float>& src, int cols, const Rect& r, std::vector<float>& out);
        void writeRect(const Rect& r, const std::vector<float>& data);

        int rows_ = 0;
        int cols_ = 0;
        std::vector<float> h_;
        std::vector<float> snapshot_;   // copia antes da edicao em curso
        bool editing_ = false;
        Rect strokeBox_;
        std::vector<Entry> undo_;
        std::vector<Entry> redo_;
    };

}
