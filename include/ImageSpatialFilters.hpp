#pragma once
#include "Image.hpp"
#include "ImageConvolution.hpp"

namespace core {

    // Mascaras do Laplaciano (Cap. 3.6.2):
    //  Neighbors4: so vizinhos ortogonais   | 0  1 0 |  1 -4 1 |  0 1 0 |
    //  Neighbors8: inclui as diagonais      | 1  1 1 |  1 -8 1 |  1 1 1 |
    enum class LaplacianKind { Neighbors4, Neighbors8 };

    // ---- Mascaras prontas (todas 'Kernel' quadrados de tamanho impar) ----

    // Media ponderada com pesos binomiais (linha do triangulo de Pascal, em
    // 2-D): para 3x3 e a mascara 1 2 1 / 2 4 2 / 1 2 1 dividida por 16
    // (Fig. 3.32b). Soma dos coeficientes = 1.
    Kernel binomialKernel(int size);

    // Mascara gaussiana: h(x, y) = exp(-(x^2 + y^2) / (2 sigma^2)) amostrada
    // ao redor do centro (Cap. 3.4.4) e dividida pela soma dos coeficientes
    // (Eq. 3.5-1). sigma controla a abertura do "sino". Com sigma <= 0 o
    // resultado tende a identidade.
    Kernel gaussianKernel(int size, double sigma);

    // Laplaciano 3x3. Com negativeCenter = false a mascara e multiplicada por -1.
    Kernel laplacianKernel(LaplacianKind kind, bool negativeCenter);

    // Sobel (Cap. 3.6.4). gx aproxima a derivada em x (ao longo das linhas):
    //   | -1 -2 -1 |        | -1 0 1 |
    //   |  0  0  0 |  gy =  | -2 0 2 |
    //   |  1  2  1 |        | -1 0 1 |
    Kernel sobelXKernel();
    Kernel sobelYKernel();

    // ---- Operacoes. Todas devolvem uma NOVA imagem com o mesmo tamanho da
    // original (a de entrada nao e alterada). ----

    // Filtro da mediana (nao linear): ordena os size*size pixels da
    // vizinhanca e usa o valor do meio (o 5o em 3x3, o 13o em 5x5, ...).
    // Tamanho invalido (par ou < 1): devolve copia sem alteracao.
    Image medianFilter(Image& img, int size, BorderMode border);

    // Laplaciano da imagem. O resultado e bruto: tem valores negativos e
    // positivos (use normalizeForDisplay para exibir).
    Image laplacian(Image& img, LaplacianKind kind, bool negativeCenter, BorderMode border);

    // Aguçamento pelo Laplaciano: g = f + c * laplaciano(f), com c = -1 se o
    // centro da mascara e negativo e c = +1 se e positivo (Eq. 3.6-4).
    // O resultado e limitado a [0, 255].
    Image sharpenLaplacian(Image& img, LaplacianKind kind, bool negativeCenter, BorderMode border);

    // Aguçamento high-boost (Cap. 3.6.3):
    //   mascara = f - f_borrada      g = f + k * mascara
    // com k = 1 temos a mascara de nitidez (unsharp masking) e com k > 1,
    // o filtro high-boost. blurKernel e o filtro de suavizacao usado para
    // obter f_borrada. O resultado e limitado a [0, 255].
    Image highBoost(Image& img, const Kernel& blurKernel, double k, BorderMode border);

    // Sobel em x e em y, separados. Aplicados por correlacao (Eq. 3.4-1),
    // como no texto. O resultado e bruto (negativos e positivos).
    Image sobelX(Image& img, BorderMode border);
    Image sobelY(Image& img, BorderMode border);

    // Magnitude do gradiente (deteccao nao linear de bordas):
    //   exata:        M = raiz(gx^2 + gy^2)
    //   aproximada:   M = |gx| + |gy|
    // O resultado e bruto (>= 0, pode passar de 255).
    Image gradientMagnitude(Image& img, BorderMode border, bool approximate);

}
