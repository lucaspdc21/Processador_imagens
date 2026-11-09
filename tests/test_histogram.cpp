// Testes do histograma (contagem, estatisticas, equalizacao).
#include "test_common.hpp"
#include "ImageColor.hpp"
#include "ImageHistogram.hpp"

using namespace core;

// Mediana exata, em inteiros: menor k com 2 * soma(nj) >= MN
static int medianExact(const Histogram& h) {
    long long c = 0;
    for (size_t k = 0; k < h.counts.size(); ++k) {
        c += h.counts[k];
        if (2 * c >= h.totalPixels) return static_cast<int>(k);
    }
    return 0;
}

TEST(mediana_em_empate_exato_em_1_2_nao_pula_nivel) {
    // Caso em que somar p(rj) em double dava 0,4999... no nivel 40 e a mediana ia para 60
    Histogram h;
    h.counts.assign(256, 0);
    const long long c[] = {6, 9, 6, 9, 7, 4, 1};
    long long total = 0;
    for (int i = 0; i < 7; ++i) { h.counts[static_cast<size_t>(i * 20)] = c[i]; total += c[i]; }
    h.totalPixels = total;
    CHECK(computeHistogramStats(h).median == 40);
    CHECK(computeHistogramStats(h).median == medianExact(h));
}

TEST(mediana_coincide_com_a_exata_em_histogramas_variados) {
    unsigned s = 12345;
    auto rnd = [&]() { s = s * 1664525u + 1013904223u; return s >> 16; };
    for (int t = 0; t < 5000; ++t) {
        Histogram h;
        h.counts.assign(256, 0);
        long long total = 0;
        const int bins = 2 + static_cast<int>(rnd() % 8);
        for (int i = 0; i < bins; ++i) {
            const long long n = 1 + static_cast<long long>(rnd() % 9);
            h.counts[static_cast<size_t>(i * 20)] = n;
            total += n;
        }
        h.totalPixels = total;
        CHECK(computeHistogramStats(h).median == medianExact(h));
    }
}

TEST(imagem_cinza_tem_os_quatro_histogramas_identicos) {
    ColorImage g(8, 8);
    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 8; ++x) {
            const int v = (x * 31 + y * 17) % 256;
            g.r().pixel(y, x) = g.g().pixel(y, x) = g.b().pixel(y, x) = v;
        }
    CHECK(g.isGray());
    const ColorHistograms h = computeColorHistograms(g);
    CHECK(h.r.counts == h.g.counts);
    CHECK(h.r.counts == h.b.counts);
    CHECK(h.r.counts == h.i.counts);

    g.r().pixel(0, 0) += 1;   // um pixel fora do cinza ja deixa de ser "cinza"
    CHECK(!g.isGray());
}

int main() { return testing::runAll(); }
