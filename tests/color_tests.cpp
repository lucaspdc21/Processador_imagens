// Testes do nucleo de cor: tabela de validacao RGB <-> HSI (Cap. 6.2.3),
// ida e volta, cinza, negativo, ajustes, equalizacao e chroma-key.
// Compilar/rodar:  cmake --build build --target color_tests && ./build/color_tests
#include "ColorImage.hpp"
#include "ColorSpace.hpp"
#include "ImageColor.hpp"
#include "ImageIntensity.hpp"
#include <cmath>
#include <cstdio>

using namespace core;

static int failures = 0;

static void check(bool ok, const char* what) {
    if (!ok) {
        ++failures;
        std::printf("FALHOU: %s\n", what);
    }
}

static bool near(double a, double b, double tol) { return std::fabs(a - b) <= tol; }

static void testTable() {
    struct Row { double r, g, b, h, s, i; };
    const Row rows[] = {
        {1, 0, 0, 0, 1, 1.0 / 3.0},
        {1, 1, 0, 60, 1, 2.0 / 3.0},
        {0, 1, 0, 120, 1, 1.0 / 3.0},
        {0, 0, 1, 240, 1, 1.0 / 3.0},
        {0.5, 0.5, 0.5, 0, 0, 0.5},
        {0, 0, 0, 0, 0, 0},
        {1, 1, 1, 0, 0, 1},
    };
    for (const Row& t : rows) {
        Hsi p = rgbToHsi(t.r, t.g, t.b);
        check(near(p.h, t.h, 1e-6) && near(p.s, t.s, 1e-9) && near(p.i, t.i, 1e-9), "tabela RGB -> HSI");
    }
}

static void testRoundTripPixels() {
    double worst = 0.0;
    for (int r = 0; r <= 255; r += 5) {
        for (int g = 0; g <= 255; g += 5) {
            for (int b = 0; b <= 255; b += 5) {
                Hsi p = rgbToHsi(r / 255.0, g / 255.0, b / 255.0);
                double r2, g2, b2;
                hsiToRgb(p, r2, g2, b2);
                worst = std::max(worst, std::max(std::fabs(r2 - r / 255.0),
                                 std::max(std::fabs(g2 - g / 255.0), std::fabs(b2 - b / 255.0))));
            }
        }
    }
    std::printf("maior erro na ida e volta RGB->HSI->RGB: %.3g\n", worst);
    check(worst < 1e-3, "ida e volta RGB -> HSI -> RGB < 1e-3");
}

static ColorImage gradientImage() {
    ColorImage img(32, 32);
    for (int y = 0; y < 32; ++y) {
        for (int x = 0; x < 32; ++x) {
            img.r().pixel(y, x) = x * 8;
            img.g().pixel(y, x) = y * 8;
            img.b().pixel(y, x) = (x * y) % 256;
        }
    }
    return img;
}

static void testRoundTripImage() {
    ColorImage img = gradientImage();
    ColorImage back = hsiToRgb(rgbToHsi(img), img);
    int worst = 0;
    for (int c = 0; c < 3; ++c)
        for (int y = 0; y < 32; ++y)
            for (int x = 0; x < 32; ++x)
                worst = std::max(worst, std::abs(back.channel(c).pixel(y, x) - img.channel(c).pixel(y, x)));
    check(worst <= 1, "ida e volta da imagem inteira (erro maximo de 1 nivel)");
}

static void testGrayAndNegative() {
    ColorImage img(1, 2);
    img.r().pixel(0, 0) = 255; img.g().pixel(0, 0) = 0;   img.b().pixel(0, 0) = 0;
    img.r().pixel(0, 1) = 10;  img.g().pixel(0, 1) = 20;  img.b().pixel(0, 1) = 30;

    ColorImage avg = img;
    toGrayAverage(avg);
    check(avg.isGray() && avg.r().pixel(0, 0) == 85 && avg.r().pixel(0, 1) == 20, "cinza: media simples");

    ColorImage w = img;
    toGrayWeighted(w, 0.299, 0.587, 0.114);
    check(w.isGray() && w.r().pixel(0, 0) == 76, "cinza: media ponderada (BT.601)");

    ColorImage neg = img;
    applyToPlanes(neg, ColorTarget::ChannelsRGB, [](Image& p) { negative(p); });
    check(neg.r().pixel(0, 0) == 0 && neg.g().pixel(0, 0) == 255 && neg.b().pixel(0, 1) == 225, "negativo por canal");
}

static void testEqualizationKeepsHue() {
    // imagem escura com cor: a equalizacao em HSI nao pode mudar H
    ColorImage img(16, 16);
    for (int y = 0; y < 16; ++y)
        for (int x = 0; x < 16; ++x) {
            img.r().pixel(y, x) = 20 + x * 2;
            img.g().pixel(y, x) = 10 + y;
            img.b().pixel(y, x) = 5;
        }
    HsiImage before = rgbToHsi(img);
    ColorImage eq = img;
    equalizeHistogramHSI(eq);
    HsiImage after = rgbToHsi(eq);
    // O matiz so pode mudar onde o RGB final foi limitado a [0, 255] (cor fora
    // do gamut ao aumentar I) ou onde o valor de 8 bits e tao pequeno que a
    // quantizacao domina. Nos demais pixels H tem de ser preservado.
    double worst = 0.0;
    int compared = 0;
    for (int y = 0; y < 16; ++y) {
        for (int x = 0; x < 16; ++x) {
            const int r = eq.r().pixel(y, x), g = eq.g().pixel(y, x), b = eq.b().pixel(y, x);
            const int mx = std::max(r, std::max(g, b)), mn = std::min(r, std::min(g, b));
            if (mx >= 255 || mn <= 0 || mx < 60) continue;
            const size_t k = before.index(y, x);
            double d = std::fabs(before.h[k] - after.h[k]);
            worst = std::max(worst, std::min(d, 360.0 - d));
            ++compared;
        }
    }
    std::printf("equalizacao: %d pixels comparados, maior variacao de matiz: %.3g graus\n", compared, worst);
    check(compared > 0 && worst < 3.0, "equalizacao HSI preserva o matiz");
    HsiImage hb = before, ha = after;
    double mb = 0, ma = 0;
    for (size_t k = 0; k < hb.i.size(); ++k) { mb += hb.i[k]; ma += ha.i[k]; }
    check(ma > mb, "equalizacao clareia imagem escura");
}

static void testAdjusts() {
    ColorImage img = gradientImage();
    ColorImage same = img;
    adjustHsi(same, 0.0, 1.0, 1.0);
    int worst = 0;
    for (int c = 0; c < 3; ++c)
        for (int y = 0; y < 32; ++y)
            for (int x = 0; x < 32; ++x)
                worst = std::max(worst, std::abs(same.channel(c).pixel(y, x) - img.channel(c).pixel(y, x)));
    check(worst <= 1, "ajuste HSI neutro nao altera a imagem");

    ColorImage red(1, 1);
    red.r().pixel(0, 0) = 255;
    adjustHsi(red, 120.0, 1.0, 1.0);   // vermelho + 120 graus = verde
    check(red.g().pixel(0, 0) == 255 && red.r().pixel(0, 0) == 0 && red.b().pixel(0, 0) == 0, "matiz +120: vermelho -> verde");

    ColorImage col(1, 1);
    col.r().pixel(0, 0) = 200; col.g().pixel(0, 0) = 100; col.b().pixel(0, 0) = 50;
    ColorImage desat = col;
    adjustHsi(desat, 0.0, 0.0, 1.0);
    check(desat.isGray(), "saturacao 0 gera cinza");

    ColorImage ch = col;
    adjustChannels(ch, 100, -150, 0);
    check(ch.r().pixel(0, 0) == 255 && ch.g().pixel(0, 0) == 0 && ch.b().pixel(0, 0) == 50, "ajuste de canal com saturacao");
}

static void testChromaKey() {
    ColorImage front(2, 2), back(1, 1);
    // (0,0) verde puro, (0,1) quase verde, (1,0) vermelho, (1,1) azul
    const int px[4][3] = {{0, 255, 0}, {20, 235, 10}, {255, 0, 0}, {0, 0, 255}};
    for (int k = 0; k < 4; ++k) {
        front.r().pixel(k / 2, k % 2) = px[k][0];
        front.g().pixel(k / 2, k % 2) = px[k][1];
        front.b().pixel(k / 2, k % 2) = px[k][2];
    }
    back.r().pixel(0, 0) = 7; back.g().pixel(0, 0) = 8; back.b().pixel(0, 0) = 9;

    ChromaKeyParams p;
    p.tolerance = 0.20;   // D0 ~ 88: pega os dois verdes
    ColorImage out = front;
    chromaKey(out, back, p);   // fundo 1x1 e ampliado para 2x2
    check(out.r().pixel(0, 0) == 7 && out.g().pixel(0, 1) == 8 && out.b().pixel(0, 1) == 9, "chroma-key troca os verdes");
    check(out.r().pixel(1, 0) == 255 && out.b().pixel(1, 1) == 255, "chroma-key mantem as outras cores");

    p.tolerance = 0.0;   // so o verde exato
    out = front;
    chromaKey(out, back, p);
    check(out.r().pixel(0, 0) == 7 && out.r().pixel(0, 1) == 20, "tolerancia 0: so o verde exato");

    p.mode = ChromaKeyMode::Cube;
    p.tolerance = 0.10;   // meia-largura ~25 por canal: pega (20,235,10)
    out = front;
    chromaKey(out, back, p);
    check(out.r().pixel(0, 1) == 7, "modo cubo");
}

static void testHistogramsAndMisc() {
    ColorImage img = gradientImage();
    ColorHistograms h = computeColorHistograms(img);
    check(h.r.totalPixels == 1024 && h.i.totalPixels == 1024, "histogramas R, G, B, I");
    long long s = 0;
    for (long long v : h.i.counts) s += v;
    check(s == 1024, "soma do histograma de I");

    ColorImage sep = img;
    SepiaParams sp;
    sepia(sep, sp);
    check(sep.r().pixel(10, 10) >= sep.b().pixel(10, 10), "sepia (matriz): R >= B");
    sp.mode = SepiaMode::HSI;
    sep = img;
    sepia(sep, sp);
    check(sep.r().pixel(10, 10) >= sep.g().pixel(10, 10) && sep.g().pixel(10, 10) >= sep.b().pixel(10, 10), "sepia (HSI): R >= G >= B");

    ColorImage g = gradientImage();
    ColorImage mag = gradientMagnitudeColor(g, BorderMode::Replicate, false, true);
    check(mag.isGray() && mag.getRows() == 32, "gradiente de Di Zenzo");

    ColorImage hv = makeView(img, ColorView::H);
    check(hv.isGray(), "visualizacao do plano H");
}

int main() {
    testTable();
    testRoundTripPixels();
    testRoundTripImage();
    testGrayAndNegative();
    testEqualizationKeepsHue();
    testAdjusts();
    testChromaKey();
    testHistogramsAndMisc();
    if (failures == 0) std::printf("OK: todos os testes passaram.\n");
    return failures == 0 ? 0 : 1;
}
