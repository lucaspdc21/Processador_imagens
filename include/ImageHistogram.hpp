#pragma once
#include "Image.hpp"
#include <vector>

namespace core {

    // Histograma de uma imagem com L niveis de intensidade: h(rk) = nk,
    // onde nk e o numero de pixels com intensidade rk
    struct Histogram {
        std::vector<long long> counts;  // counts[k] = nk, com L posicoes
        long long totalPixels = 0;      // MN (numero total de pixels)

        int levels() const { return static_cast<int>(counts.size()); }
    };

    // Media e variancia calculadas a partir do histograma normalizado (Eq. 3.3-18 e 3.3-19). A media mede a intensidade media e o desvio
    struct HistogramStats {
        double mean = 0.0;
        double variance = 0.0;
        double stdDev = 0.0;
    };

    // Calcula h(rk) = nk
    Histogram computeHistogram(Image& img, int levels = 256);

    // Histograma normalizado: p(rk) = nk / MN (Eq. 3.3-7). A soma e 1.
    std::vector<double> normalizeHistogram(const Histogram& h);

    HistogramStats computeHistogramStats(const Histogram& h);

    // Funcao de transformacao da equalizacao de histograma (Eq. 3.3-8):
    //   sk = T(rk) = (L - 1) * soma_{j=0..k} p(rj)
    // arredondada para o inteiro mais proximo. Retorna L valores, onde
    // mapping[k] e o novo nivel de intensidade dos pixels com intensidade k.
    std::vector<int> equalizationMapping(const Histogram& h);

    // Equaliza o histograma da imagem (8 bits, L = 256) aplicando a tabela
    // calculada por equalizationMapping.
    void equalizeHistogram(Image& img);

}
