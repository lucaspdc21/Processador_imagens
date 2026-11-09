// Testes das tabelas usadas pelo grafico das curvas log e gama (TransformCurve.hpp).
#include "test_common.hpp"
#include "Image.hpp"
#include "ImageIntensity.hpp"
#include "TransformCurve.hpp"
#include <cmath>

using namespace core;

// Imagem 16 x 16 com os 256 niveis (um de cada), para aplicar a funcao real do core
static Image allLevels() {
    Image img(16, 16);
    for (int r = 0; r < 256; ++r) img.pixel(r / 16, r % 16) = r;
    return img;
}

TEST(curva_gama_coincide_com_a_funcao_aplicada_na_imagem) {
    const double gammas[] = {0.2, 0.5, 1.0, 2.2, 4.0};
    const double cs[] = {0.5, 1.0, 1.7};
    for (double g : gammas) {
        for (double c : cs) {
            Image img = allLevels();
            gammaCorrection(img, g, c);
            const ui::CurveLut lut = ui::gammaCurveLut(g, c);
            for (int r = 0; r < 256; ++r) CHECK(lut[static_cast<size_t>(r)] == img.pixel(r / 16, r % 16));
        }
    }
}

TEST(curva_log_coincide_com_a_funcao_aplicada_na_imagem) {
    const double cs[] = {0.5, 10.0, 46.0, 100.0};
    for (double c : cs) {
        Image img = allLevels();
        logTransform(img, c, false);
        const ui::CurveLut lut = ui::logCurveLut(c);
        for (int r = 0; r < 256; ++r) CHECK(lut[static_cast<size_t>(r)] == img.pixel(r / 16, r % 16));
    }
}

TEST(curva_gama_1_e_identidade_e_curva_log_automatica_chega_a_255) {
    const ui::CurveLut id = ui::gammaCurveLut(1.0, 1.0);
    for (int r = 0; r < 256; ++r) CHECK(id[static_cast<size_t>(r)] == r);

    // c automatico para maximo 255: s(0) = 0 e s(255) = 255, e a curva e crescente
    const double c = 255.0 / std::log(1.0 + 255.0);
    const ui::CurveLut lg = ui::logCurveLut(c);
    CHECK(lg[0] == 0);
    CHECK(lg[255] == 255);
    for (int r = 1; r < 256; ++r) CHECK(lg[static_cast<size_t>(r)] >= lg[static_cast<size_t>(r - 1)]);
}

int main() { return testing::runAll(); }
