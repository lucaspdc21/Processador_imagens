#pragma once
#include "ColorImage.hpp"
#include "ColorSpace.hpp"
#include "ImageConvolution.hpp"
#include "ImageHistogram.hpp"
#include <functional>

namespace core {

    // =====================================================================
    // Processamento de imagens em escala de cinza aplicado a imagens coloridas
    // (Cap. 6.4, 6.6, 6.7.3 e 6.8)
    //
    //  ChannelsRGB:  a mesma operacao em R, G e B, separadamente. E
    //                equivalente ao processamento vetorial quando a operacao
    //                em cada componente independe das demais (negativo,
    //                gama, log, contraste, suavizacao, Laplaciano...).
    //  IntensityHSI: converte para HSI, aplica a operacao so no plano I e
    //                converte de volta. H e S nao mudam (cores preservadas).
    // =====================================================================
    enum class ColorTarget { ChannelsRGB, IntensityHSI };

    using PlaneOp = std::function<void(Image&)>;    // altera o plano no lugar
    using PlaneFn = std::function<Image(Image&)>;   // devolve um novo plano

    // Aplica 'op' em cada plano (ChannelsRGB) ou so no plano I (IntensityHSI).
    // No modo IntensityHSI o plano I vai para [0, 255] (8 bits), 'op' e
    // aplicada e o resultado e limitado a [0, 255] antes de voltar a RGB.
    void applyToPlanes(ColorImage& img, ColorTarget target, const PlaneOp& op);

    // Igual a applyToPlanes, mas 'fn' devolve a imagem filtrada (filtros
    // espaciais que retornam uma nova imagem do mesmo tamanho).
    void mapPlanes(ColorImage& img, ColorTarget target, const PlaneFn& fn);

    // ---- Escala de cinza (saida com 3 canais iguais, R = G = B) ----

    // Media aritmetica simples: Y = (R + G + B) / 3 (e o I do HSI, Eq. 6.2-4)
    void toGrayAverage(ColorImage& img);

    // Media ponderada: Y = wR R + wG G + wB B. Os pesos sao normalizados pela
    // soma. Se a soma for <= 0, usa ITU-R BT.601 (0.299, 0.587, 0.114).
    void toGrayWeighted(ColorImage& img, double wR = 0.299, double wG = 0.587, double wB = 0.114);

    // ---- Sepia ----
    enum class SepiaMode {
        Matrix,   // matriz classica
        HSI       // Y ponderado + matiz e saturacao fixos
    };

    struct SepiaParams {
        SepiaMode mode = SepiaMode::Matrix;
        double hue = 35.0;          // graus (so no modo HSI): ~30 a 40
        double saturation = 0.30;   // [0, 1] (so no modo HSI): ~0.25 a 0.40
    };

    void sepia(ColorImage& img, const SepiaParams& params);

    // ---- Chroma-key ----
    enum class ChromaKeyMode {
        Sphere,   // distancia euclidiana no RGB (Eq. 6.7-1)
        Cube      // distancia de Chebyshev: cubo de meia-largura W/2 (Eq. 6.5-7)
    };

    struct ChromaKeyParams {
        int keyR = 0;
        int keyG = 255;
        int keyB = 0;                  // cor-chave a = (0, 255, 0)
        double tolerance = 0.40;       // D0 em [0, 1]: 1 = distancia maxima possivel
        double feather = 0.0;          // [0, 1]: largura da borda suave (0 = mascara dura)
        ChromaKeyMode mode = ChromaKeyMode::Sphere;
    };

    // Distancia maxima no RGB de 8 bits: sqrt(3)*255 (esfera) ou 255 (cubo)
    double chromaKeyMaxDistance(ChromaKeyMode mode);

    // Substitui os pixels de 'front' proximos da cor-chave pelos pixels de
    // 'background'. O fundo e redimensionado (bilinear) para o tamanho de
    // 'front' se for diferente. A frente e alterada no lugar.
    //   D(z, a) <= D0  -> pixel do fundo
    //   caso contrario -> pixel da frente
    // Com feather > 0 a transicao e suave (mistura) entre D0 e D0 + feather.
    void chromaKey(ColorImage& front, ColorImage& background, const ChromaKeyParams& params);

    // Media do RGB numa janela (2*radius+1)^2 centrada em (row, col), para
    // escolher a cor-chave com o conta-gotas. Retorna false se fora da imagem.
    bool sampleAverageColor(ColorImage& img, int row, int col, int radius, int& r, int& g, int& b);

    // ---- Histogramas R, G, B e I ----
    struct ColorHistograms {
        Histogram r, g, b, i;   // I = round((R + G + B) / 3), 8 bits
    };
    ColorHistograms computeColorHistograms(ColorImage& img);

    // ---- Equalizacao de histograma em imagem colorida (Cap. 6.5.5) ----
    // Equaliza so o plano I do HSI e mantem H e S, o que preserva as cores
    // (equalizar R, G e B separadamente altera os matizes). saturationGain
    // multiplica S depois (a imagem equalizada perde vivacidade): 1 = nao muda.
    void equalizeHistogramHSI(ColorImage& img, double saturationGain = 1.0);

    // ---- Ajustes em HSI (Cap. 6.2.3 e 6.5.1) ----
    // Uma unica ida e volta ao HSI:
    //   H' = (H + deltaHue) mod 360,  S' = clip(S * saturationFactor, 0, 1),
    //   I' = clip(I * intensityFactor, 0, 1)
    void adjustHsi(ColorImage& img, double deltaHue, double saturationFactor, double intensityFactor);

    // ---- Ajuste de canal C/R, M/G, Y/B (Cap. 6.2.2 e 6.5.4) ----
    // Cada cor e complementar da outra: C = 1 - R, M = 1 - G, Y = 1 - B.
    // Somar delta ao canal R e o mesmo que subtrair delta de C, e assim por
    // diante: R' = clip(R + dR), G' = clip(G + dG), B' = clip(B + dB).
    // delta positivo = mais Vermelho / Verde / Azul; negativo = mais Ciano /
    // Magenta / Amarelo. Em niveis de 0 a 255 (-255 a +255).
    void adjustChannels(ColorImage& img, int dR, int dG, int dB);

    // ---- Bordas em imagem colorida ----
    // Magnitude do gradiente (Sobel). Retorna uma imagem cinza (R = G = B)
    // com valores brutos (>= 0, podem passar de 255; use normalizeForDisplay).
    //  diZenzo = false: calcula por canal e soma as magnitudes (Fig. 6.46c)
    //  diZenzo = true:  gradiente vetorial de Di Zenzo (Eqs. 6.7-3 a 6.7-9)
    ColorImage gradientMagnitudeColor(ColorImage& img, BorderMode border, bool approximate, bool diZenzo);

    // ---- Visualizacao dos planos ----
    enum class ColorView { Color, R, G, B, H, S, I };

    // Cor: copia da imagem. R, G, B, H, S ou I: o plano em cinza (3 canais iguais).
    ColorImage makeView(ColorImage& img, ColorView view);

}
