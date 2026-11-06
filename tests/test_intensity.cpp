// Testes da transformacao linear definida por partes (alargamento de contraste).
#include "test_common.hpp"
#include "Image.hpp"
#include "ImageIntensity.hpp"
#include <cmath>

using namespace core;

TEST(contrast_stretch_value_passa_pelos_pontos_de_controle) {
    CHECK(std::fabs(contrastStretchValue(0, 70, 20, 180, 230) - 0.0) < 1e-9);
    CHECK(std::fabs(contrastStretchValue(70, 70, 20, 180, 230) - 20.0) < 1e-9);
    CHECK(std::fabs(contrastStretchValue(180, 70, 20, 180, 230) - 230.0) < 1e-9);
    CHECK(std::fabs(contrastStretchValue(255, 70, 20, 180, 230) - 255.0) < 1e-9);
}

TEST(contrast_stretch_value_identidade) {
    for (int r = 0; r < 256; ++r)
        CHECK(std::fabs(contrastStretchValue(r, 85, 85, 170, 170) - r) < 1e-9);
}

TEST(contrast_stretch_imagem_coincide_com_a_curva) {
    Image img(1, 256);
    for (int c = 0; c < 256; ++c) img.pixel(0, c) = c;
    contrastStretch(img, 70, 0, 180, 255);
    for (int c = 0; c < 256; ++c) {
        const double t = contrastStretchValue(c, 70, 0, 180, 255);
        CHECK(img.pixel(0, c) == static_cast<int>(std::lround(t)));
    }
    CHECK(img.pixel(0, 70) == 0);
    CHECK(img.pixel(0, 180) == 255);
}

TEST(contrast_stretch_value_limites_invalidos) {
    // fora de [0,255] e r1 > r2 sao normalizados como em contrastStretch
    CHECK(contrastStretchValue(100, 300, 0, -5, 255) == contrastStretchValue(100, 255, 0, 0, 255));
    CHECK(contrastStretchValue(100, 180, 255, 70, 0) == contrastStretchValue(100, 70, 255, 180, 0));
}

int main() { return testing::runAll(); }
