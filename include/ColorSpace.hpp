#pragma once
#include "ColorImage.hpp"
#include <vector>

namespace core {

    // Pixel no espaco HSI (Cap. 6.2.3)
    //  h: matiz em graus, [0, 360)  (0 quando S = 0: o matiz e indefinido)
    //  s: saturacao, [0, 1]
    //  i: intensidade, [0, 1]
    struct Hsi {
        double h = 0.0;
        double s = 0.0;
        double i = 0.0;
    };

    // Imagem HSI: tres planos em ponto flutuante (linha a linha)
    struct HsiImage {
        int rows = 0;
        int cols = 0;
        std::vector<float> h;   // graus
        std::vector<float> s;
        std::vector<float> i;

        size_t index(int row, int col) const {
            return static_cast<size_t>(row) * static_cast<size_t>(cols) + static_cast<size_t>(col);
        }
    };

    enum class HsiPlane { H, S, I };

    // ---- Conversao de um pixel. R, G, B e I em [0, 1]. ----

    // RGB -> HSI (Eqs. 6.2-2 a 6.2-4)
    Hsi rgbToHsi(double r, double g, double b);

    // HSI -> RGB (Eqs. 6.2-5 a 6.2-15). A saida e limitada a [0, 1].
    void hsiToRgb(const Hsi& hsi, double& r, double& g, double& b);

    // ---- Conversao da imagem inteira (R, G, B de 0 a 255) ----
    HsiImage rgbToHsi(ColorImage& img);

    // Converte de volta; metadados (dpi, bits) vem de 'like'. Cada canal e
    // arredondado para o inteiro mais proximo e limitado a [0, 255].
    ColorImage hsiToRgb(const HsiImage& hsi, ColorImage& like);

    // Um plano do HSI como imagem em cinza (3 canais iguais) para exibicao:
    // H/360, S e I mapeados para [0, 255] (Figs. 6.15 e 6.16).
    // Observacao: a imagem de H tem descontinuidade em 0/360 graus
    // (vermelho), isso e esperado.
    Image hsiPlaneToImage(const HsiImage& hsi, HsiPlane plane);

}
