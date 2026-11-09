#pragma once
#include "Image.hpp"
#include "ImageHistogram.hpp"
#include "ImageIntensity.hpp"
#include <array>

namespace ui {

    // Tabela s = T(r) para r = 0..255 (lut[r] e o nivel de saida do nivel de entrada r).
    using CurveLut = std::array<int, 256>;

    namespace detail {
        // Amostra uma transformacao de intensidade aplicando-a a uma rampa 0..255.
        // Assim a curva desenhada e exatamente a funcao do core que e usada na
        // imagem (com o mesmo arredondamento e a mesma saturacao), sem copiar formulas.
        template <typename Fn>
        CurveLut sampleTransform(Fn&& fn) {
            core::Image ramp(1, 256);
            for (int r = 0; r < 256; ++r) ramp.pixel(0, r) = r;
            fn(ramp);
            CurveLut lut{};
            for (int r = 0; r < 256; ++r) lut[static_cast<size_t>(r)] = ramp.pixel(0, r);
            return lut;
        }
    }

    // s = c * ln(1 + r)   (core::logTransform com a constante c ja resolvida)
    inline CurveLut logCurveLut(double c) {
        return detail::sampleTransform([&](core::Image& img) { core::logTransform(img, c, false); });
    }

    // s = 255 * c * (r / 255)^gama   (core::gammaCorrection)
    inline CurveLut gammaCurveLut(double gamma, double c) {
        return detail::sampleTransform([&](core::Image& img) { core::gammaCorrection(img, gamma, c); });
    }

    // Grafico interativo da curva de transformacao T(r): r (entrada) no eixo x e
    // s (saida) no eixo y, ambos em [0, 255]. Mostra:
    //   - a curva (cor 'curveColor', formato IM_COL32) e a reta identidade s = r;
    //   - ao fundo, o histograma de 'backdrop' (opcional) em escala logaritmica,
    //     para ver onde estao os pixels em relacao a curva;
    //   - as faixas de entrada que saturam em 255 (vermelho) ou vao a 0 (azul);
    //   - ao passar o mouse: guias, ponto na curva e tooltip com r -> s e a
    //     quantidade de pixels naquele nivel.
    // Nao altera nenhum parametro: os valores continuam vindo dos controles
    // existentes; o grafico so acompanha. 'side' e o lado do quadrado em pixels
    // (<= 0: usa a largura disponivel, limitada a 320).
    void TransformCurvePlot(const char* id, const CurveLut& lut, unsigned int curveColor,
                            const core::Histogram* backdrop = nullptr, float side = 0.0f);

}
