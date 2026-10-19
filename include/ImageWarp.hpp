#pragma once
#include "Image.hpp"
#include <array>

namespace core {

    // Convencoes de coordenadas (usadas por escala e rotacao):
    //  - Imagem com H linhas e W colunas; pixel na linha y e coluna x (0-based).
    //  - O CENTRO do pixel (x, y) esta na coordenada continua (x, y). A imagem
    //    ocupa o retangulo continuo [-0.5, W-0.5] x [-0.5, H-0.5].
    //  - O eixo y aponta para baixo.
    //  - Atencao: Image::pixel(x, y) do projeto usa x = LINHA e y = COLUNA.
    //    Nas funcoes de warp, x e sempre a COLUNA e y a LINHA (coordenadas
    //    de imagem); a conversao e feita internamente.

    enum class Interpolation {
        Nearest,    // vizinho mais proximo
        Bilinear    // bilinear (4 vizinhos)
    };

    // Matriz afim 3x3 em coordenadas homogeneas, vetores coluna: p' = A * p,
    // com p = [x y 1]^T (Eq. 2.6-23 / Tabela 2.2, escrita com vetores coluna).
    struct Affine2D {
        std::array<double, 9> m = {1, 0, 0,
                                   0, 1, 0,
                                   0, 0, 1};

        double& at(int r, int c) { return m[static_cast<size_t>(r * 3 + c)]; }
        double at(int r, int c) const { return m[static_cast<size_t>(r * 3 + c)]; }

        static Affine2D identity();
        static Affine2D scale(double sx, double sy);
        static Affine2D translation(double tx, double ty);
        // Rotacao em coordenadas de imagem (y para baixo), angulo em radianos:
        // R = [[cos, sin], [-sin, cos]]; com o sinal da constante de
        // ImageRotate (positivo = anti-horario visual).
        static Affine2D rotationImageCoords(double radians);

        // Concatenacao: (A * B) aplica B primeiro e depois A.
        Affine2D operator*(const Affine2D& other) const;

        // Inversa de uma transformacao afim (ultima linha [0 0 1]).
        // Retorna false se a matriz for singular.
        bool inverse(Affine2D& out) const;

        void apply(double x, double y, double& xo, double& yo) const {
            xo = m[0] * x + m[1] * y + m[2];
            yo = m[3] * x + m[4] * y + m[5];
        }
    };

    // Amostragem em coordenadas reais (xs = coluna, ys = linha).
    // Vizinho mais proximo: xi = floor(xs + 0.5), yi = floor(ys + 0.5).
    // Os indices que saem da imagem sao limitados (clamp) a [0, W-1], [0, H-1].
    double sampleNearest(Image& img, double xs, double ys);

    // Bilinear (Eq. 2.4-6), em forma de pesos:
    //   v = (1-dx)(1-dy) I[y0,x0] + dx(1-dy) I[y0,x0+1]
    //     + (1-dx)dy I[y0+1,x0]   + dx dy I[y0+1,x0+1]
    // Vizinhos x0+1 / y0+1 fora da imagem sao limitados (clamp).
    double sampleBilinear(Image& img, double xs, double ys);

    // Warp por MAPEAMENTO INVERSO (Secao 2.6.5): percorre os pixels da saida
    // (Ho x Wo); para cada (xo, yo) calcula (xs, ys) = A_inv * (xo, yo, 1) na
    // entrada e interpola. Pontos fora do dominio valido
    // (-0.5 <= xs <= W-0.5 e -0.5 <= ys <= H-0.5) recebem o valor de fundo.
    // Dentro do dominio os vizinhos usam clamp (bordas nao escurecem).
    //
    // O resultado e arredondado para o inteiro mais proximo. Como a
    // interpolacao e uma combinacao convexa dos vizinhos, o resultado fica na
    // faixa dos valores de entrada (nao e necessario limitar a [0, 255]).
    //
    // Nao ha limite de tamanho aqui; quem chama deve validar (ver
    // kMaxOutputPixels). Retorna imagem vazia (0 x 0) se Ho ou Wo <= 0.
    Image warpInverse(Image& src, const Affine2D& aInv, int outRows, int outCols,
                      Interpolation interp, int background);

    // Limite de seguranca para o tamanho da saida (100 megapixels).
    constexpr long long kMaxOutputPixels = 100LL * 1000LL * 1000LL;

}
