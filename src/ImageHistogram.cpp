#include "ImageHistogram.hpp"
#include <algorithm>
#include <cmath>

namespace core {

Histogram computeHistogram(Image& img, int levels) {
    if (levels < 2) levels = 2;

    Histogram h;
    h.counts.assign(static_cast<size_t>(levels), 0);

    for (auto& row : img.getData()) {
        for (int v : row) {
            v = std::max(0, std::min(v, levels - 1));
            ++h.counts[static_cast<size_t>(v)];
        }
    }

    h.totalPixels = static_cast<long long>(img.getRows()) * img.getCols();
    return h;
}

std::vector<double> normalizeHistogram(const Histogram& h) {
    std::vector<double> p(h.counts.size(), 0.0);
    if (h.totalPixels <= 0) return p;

    for (size_t k = 0; k < h.counts.size(); ++k) {
        p[k] = static_cast<double>(h.counts[k]) / static_cast<double>(h.totalPixels);
    }
    return p;
}

HistogramStats computeHistogramStats(const Histogram& h) {
    HistogramStats s;
    std::vector<double> p = normalizeHistogram(h);

    // m = soma ri * p(ri)
    for (size_t i = 0; i < p.size(); ++i) {
        s.mean += static_cast<double>(i) * p[i];
    }
    // variancia = soma (ri - m)^2 * p(ri)
    for (size_t i = 0; i < p.size(); ++i) {
        double d = static_cast<double>(i) - s.mean;
        s.variance += d * d * p[i];
    }
    s.stdDev = std::sqrt(s.variance);

    // mediana: primeiro nivel em que a distribuicao acumulada chega a 1/2
    double cumulative = 0.0;
    for (size_t i = 0; i < p.size(); ++i) {
        cumulative += p[i];
        if (cumulative >= 0.5) {
            s.median = static_cast<int>(i);
            break;
        }
    }
    return s;
}

std::vector<int> equalizationMapping(const Histogram& h) {
    int L = h.levels();
    std::vector<int> mapping(static_cast<size_t>(L), 0);

    // Sem pixels nao ha o que equalizar: mantem a identidade
    if (h.totalPixels <= 0) {
        for (int k = 0; k < L; ++k) mapping[static_cast<size_t>(k)] = k;
        return mapping;
    }

    const long long MN = h.totalPixels;
    long long cumulative = 0;  // soma de nj para j = 0..k (CDF sem normalizar)

    for (int k = 0; k < L; ++k) {
        cumulative += h.counts[static_cast<size_t>(k)];

        // sk = (L - 1) * cumulative / MN, arredondado para o inteiro mais
        // proximo. Feito em aritmetica inteira para nao haver erro de
        // ponto flutuante nos empates (x.5).
        long long s = ((L - 1) * cumulative * 2 + MN) / (2 * MN);
        mapping[static_cast<size_t>(k)] = static_cast<int>(std::max(0LL, std::min(s, static_cast<long long>(L - 1))));
    }
    return mapping;
}

void equalizeHistogram(Image& img) {
    Histogram h = computeHistogram(img, 256);
    std::vector<int> lut = equalizationMapping(h);

    for (int row = 0; row < img.getRows(); ++row) {
        for (int col = 0; col < img.getCols(); ++col) {
            int r = std::max(0, std::min(img.pixel(row, col), 255));
            img.pixel(row, col) = lut[static_cast<size_t>(r)];
        }
    }
}

}
