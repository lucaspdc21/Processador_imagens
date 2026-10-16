#pragma once
#include "Image.hpp"
#include <vector>

namespace core {

    // Filtro espacial (mascara, kernel) quadrado de tamanho m x m, com m impar:
    // m = 2a + 1 (Cap. 3.4.1). Os coeficientes w(s, t), com s (linhas) e
    // t (colunas) em [-a, a], ficam guardados linha a linha; w(0, 0) e o
    // coeficiente central, que se alinha com o pixel (x, y) da imagem.
    struct Kernel {
        int size = 3;
        std::vector<double> weights = std::vector<double>(9, 0.0);   // size * size

        Kernel() = default;
        explicit Kernel(int kernelSize)
            : size(kernelSize),
              weights(kernelSize > 0 ? static_cast<size_t>(kernelSize) * static_cast<size_t>(kernelSize) : 0, 0.0) {}

        int radius() const { return size / 2; }   // a

        double& w(int s, int t) {
            return weights[static_cast<size_t>((s + radius()) * size + (t + radius()))];
        }
        double w(int s, int t) const {
            return weights[static_cast<size_t>((s + radius()) * size + (t + radius()))];
        }
    };

    // Convolucao (Eq. 3.4-2): a mascara e girada 180 graus antes da soma de produtos.
    // Correlacao (Eq. 3.4-1): soma de produtos com a mascara sem alteracoes.
    enum class FilterOperation { Convolution, Correlation };

    // Como tratar os pixels fora da imagem (preenchimento / padding).
    //  Zero:      borda de 0s (usada no texto; escurece as bordas da imagem)
    //  Replicate: repete o pixel mais proximo da borda
    enum class BorderMode { Zero, Replicate };

    // Tamanho valido: inteiro positivo e impar (1x1, 3x3, 5x5, ...).
    bool isValidKernelSize(int size);

    Kernel identityKernel(int size);   // 1 no centro e 0 nos demais
    Kernel boxKernel(int size);        // todos os coeficientes iguais a 1/(size*size)
    double kernelSum(const Kernel& kernel);
    Kernel rotateKernel180(const Kernel& kernel);

    // Filtragem espacial linear de uma imagem M x N com uma mascara m x n.
    // Retorna uma NOVA imagem com o mesmo tamanho da original: os pixels
    // filtrados nao substituem os da imagem de entrada durante o calculo
    // (nota de rodape da Secao 3.4.1).
    //
    // O resultado e arredondado para o inteiro mais proximo, mas NAO e
    // limitado a [0, 255]: dependendo da mascara ha valores negativos e
    // maiores que 255. Para exibir, use normalizeForDisplay.
    //
    // Se o kernel for invalido (tamanho par ou numero de coeficientes
    // incorreto), retorna uma copia da imagem sem alteracao.
    Image filterLinear(Image& img, const Kernel& kernel, FilterOperation operation, BorderMode border);

    // filterLinear aplicado diretamente na imagem. Se clampToByte for true,
    // o resultado e limitado a [0, maxIntensity]; senao os valores brutos sao
    // mantidos.
    void applyFilter(Image& img, const Kernel& kernel, FilterOperation operation,
                     BorderMode border, bool clampToByte);

    // Normalizacao min-max para exibicao: leva o menor valor da imagem a 0 e o
    // maior a 255 (a imagem resultante fica acinzentada quando ha valores
    // negativos). Retorna uma copia; a imagem original nao e alterada.
    // Se todos os pixels tem o mesmo valor, apenas limita esse valor a [0, 255].
    Image normalizeForDisplay(Image& img);

}
