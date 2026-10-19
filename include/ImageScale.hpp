#pragma once
#include "Image.hpp"
#include "ImageWarp.hpp"

namespace core {

    // Calcula o tamanho de saida da escala (Secao 3.5):
    //   Wo = max(1, round(sx * W)),  Ho = max(1, round(sy * H)).
    // Retorna false se sx/sy forem invalidos (<= 0, NaN, infinito) ou se a
    // saida passar de kMaxOutputPixels (100 megapixels).
    bool computeScaledSize(int rows, int cols, double sx, double sy, int& outRows, int& outCols);

    // Escala (ampliacao/reducao) por mapeamento inverso, com sx e sy
    // independentes. Usa a escala EFETIVA sx_ef = Wo/W, sy_ef = Ho/H e o
    // alinhamento por centro de pixel:
    //     xs = (xo + 0.5) / sx_ef - 0.5,   ys = (yo + 0.5) / sy_ef - 0.5
    // Assim a imagem nao e deslocada e a ultima linha/coluna e alcancada.
    //
    // antialias: se true e a escala de um eixo for < 1, aplica antes da
    // reamostragem um filtro de media (caixa) de tamanho ~ceil(1/s) nesse eixo
    // (Secao 4.5.4). O tamanho e forcado a ser impar para a caixa ficar
    // centrada (sem deslocar a imagem em meio pixel).
    //
    // Retorna imagem vazia (0 x 0) se os parametros forem invalidos.
    Image scaleImage(Image& src, double sx, double sy, Interpolation interp, bool antialias);

    // Filtro de media (caixa) separavel, tamanhos kRows (vertical) e kCols
    // (horizontal) impares; borda tratada por replicacao. Tamanho 1 = sem
    // filtragem naquele eixo. Retorna uma nova imagem.
    Image boxBlur(Image& src, int kRows, int kCols);

}
