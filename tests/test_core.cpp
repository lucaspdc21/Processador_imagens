// Testes do nucleo (sem interface). Rodar com: ctest --test-dir build --output-on-failure
#include "Image.hpp"
#include "ImageIntensity.hpp"
#include "ImageLoad.hpp"
#include "ImageMetrics.hpp"
#include "ImageQuantization.hpp"
#include "ImageResample.hpp"
#include "ImageStats.hpp"

#include <cstdio>
#include <filesystem>
#include <string>

static int failures = 0;

#define CHECK(cond)                                                              \
    do {                                                                         \
        if (!(cond)) {                                                           \
            std::printf("FALHOU %s:%d: %s\n", __FILE__, __LINE__, #cond);        \
            ++failures;                                                          \
        }                                                                        \
    } while (0)

using core::Image;

static std::string tempPath(const std::string& name) {
    return (std::filesystem::temp_directory_path() / name).string();
}

static bool sameImage(const Image& a, const Image& b) {
    return a.getRows() == b.getRows() && a.getCols() == b.getCols() &&
           a.getChannels() == b.getChannels() && a.samples() == b.samples();
}

// 16 x 16 pixels, cada nivel de cinza de 0 a 255 aparece exatamente uma vez.
static Image allGrayLevels() {
    Image img(16, 16);
    for (int i = 0; i < 256; ++i) img.at(i / 16, i % 16) = static_cast<float>(i);
    return img;
}

static Image colorImage() {
    Image img(8, 8, 255, 8, 72.0, 3);
    for (int r = 0; r < 8; ++r)
        for (int c = 0; c < 8; ++c) {
            img.at(r, c, 0) = static_cast<float>(r * 30);
            img.at(r, c, 1) = static_cast<float>(c * 30);
            img.at(r, c, 2) = static_cast<float>((r + c) * 15);
        }
    return img;
}

// Bug 1: salvar e reabrir uma imagem cinza nao pode alterar nenhum pixel.
static void testGrayRoundTrip() {
    for (const char* ext : {".png", ".bmp"}) {
        Image original = allGrayLevels();
        std::string path = tempPath(std::string("t_gray") + ext);
        CHECK(core::saveImage(path, original));
        Image loaded;
        CHECK(core::loadImage(path, loaded));
        CHECK(loaded.getChannels() == 1);
        CHECK(sameImage(original, loaded));
        std::filesystem::remove(path);
    }
}

// Um BMP cinza (gravado como RGB com R = G = B) volta com 1 canal.
static void testGrayBmpStaysGray() {
    Image gray = allGrayLevels();
    std::string path = tempPath("t_gray_ch.bmp");
    CHECK(core::saveImage(path, gray));
    Image loaded;
    CHECK(core::loadImage(path, loaded));
    CHECK(loaded.getChannels() == 1);
    std::filesystem::remove(path);
}

// Bug 2: imagens coloridas mantem os 3 canais.
static void testColorRoundTrip() {
    for (const char* ext : {".png", ".bmp"}) {
        Image original = colorImage();
        std::string path = tempPath(std::string("t_color") + ext);
        CHECK(core::saveImage(path, original));
        Image loaded;
        CHECK(core::loadImage(path, loaded));
        CHECK(loaded.getChannels() == 3);
        CHECK(sameImage(original, loaded));
        std::filesystem::remove(path);
    }
}

// Bug 3: JPG e escolhido pela extensao (com perdas, entao so conferimos o tamanho).
static void testSaveFormats() {
    Image original = colorImage();
    std::string jpg = tempPath("t_color.jpg");
    CHECK(core::saveImage(jpg, original));
    Image loaded;
    CHECK(core::loadImage(jpg, loaded));
    CHECK(loaded.getRows() == 8 && loaded.getCols() == 8 && loaded.getChannels() == 3);
    std::filesystem::remove(jpg);

    CHECK(core::hasSupportedSaveExtension("a.PNG"));
    CHECK(core::hasSupportedSaveExtension("pasta.v2/a.jpeg"));
    CHECK(core::hasSupportedSaveExtension("a.bmp"));
    CHECK(!core::hasSupportedSaveExtension("a"));
    CHECK(!core::hasSupportedSaveExtension("pasta.v2/a"));
    CHECK(core::isJpegPath("x.JPG"));
    CHECK(!core::isJpegPath("x.png"));
    CHECK(!core::loadImage(tempPath("nao_existe.png"), loaded));
}

static void testStegoInMemory() {
    Image img = allGrayLevels();
    CHECK(core::steganographyCapacityBytes(img) == (256 - 32) / 8);
    CHECK(core::embedMessageLSB(img, "ola mundo"));
    CHECK(core::extractMessageLSB(img) == "ola mundo");

    Image tooSmall(4, 4);
    CHECK(!core::embedMessageLSB(tooSmall, "x"));
    CHECK(!core::embedMessageLSB(img, ""));
    Image clean = allGrayLevels();
    CHECK(core::extractMessageLSB(clean).empty()); // imagem sem mensagem
}

// Bug 1 + 3: a mensagem sobrevive ao salvar em PNG e BMP e reabrir (cinza e RGB).
static void testStegoSurvivesLosslessFiles() {
    Image gray(32, 32);
    gray.fill(100);
    Image color(32, 32, 255, 8, 72.0, 3);
    for (int r = 0; r < 32; ++r)
        for (int c = 0; c < 32; ++c)
            for (int ch = 0; ch < 3; ++ch) color.at(r, c, ch) = static_cast<float>(50 + ch * 40);
    for (Image* img : {&gray, &color}) {
        CHECK(core::embedMessageLSB(*img, "segredo"));
        for (const char* ext : {".png", ".bmp"}) {
            std::string path = tempPath(std::string("t_stego") + ext);
            CHECK(core::saveImage(path, *img));
            Image loaded;
            CHECK(core::loadImage(path, loaded));
            CHECK(core::extractMessageLSB(loaded) == "segredo");
            std::filesystem::remove(path);
        }
    }
}

static void testIntensity() {
    Image img = allGrayLevels();
    Image copy = img;
    core::negative(copy);
    CHECK(copy.at(0, 0) == 255.0f && copy.at(15, 15) == 0.0f);
    core::negative(copy);
    CHECK(sameImage(img, copy));

    Image b = img;
    core::brightness(b, 10);
    CHECK(b.at(0, 0) == 10.0f && b.at(15, 15) == 255.0f && b.at(0, 5) == 15.0f);
    core::brightness(b, -300);
    for (float v : b.samples()) CHECK(v == 0.0f);

    Image g = img;
    core::gammaCorrection(g, 1.0, 1.0);
    CHECK(sameImage(img, g));

    Image t = img;
    core::threshold(t, 128);
    CHECK(t.at(7, 15) == 0.0f && t.at(8, 0) == 255.0f);

    Image s = img;
    core::contrastStretch(s, 50, 50, 200, 200);
    CHECK(sameImage(img, s));

    Image p = img;
    core::bitPlaneSlice(p, 8);
    CHECK(p.at(7, 15) == 0.0f && p.at(8, 0) == 255.0f);

    // As operacoes valem para cada canal da imagem colorida.
    Image c = colorImage();
    Image c2 = c;
    core::negative(c2);
    CHECK(c2.at(2, 3, 0) == 255.0f - c.at(2, 3, 0));
    CHECK(c2.at(2, 3, 2) == 255.0f - c.at(2, 3, 2));
}

static void testQuantizeAndResample() {
    Image img = allGrayLevels();
    Image q = img;
    core::quantize(q, 8);
    CHECK(sameImage(img, q));
    core::quantize(q, 1);
    CHECK(q.getBitsPerPixel() == 1);
    for (float v : q.samples()) CHECK(v == 64.0f || v == 192.0f);

    Image same = colorImage();
    Image r = same;
    core::resample(r, 8, 8, core::resampleType::Nearest);
    CHECK(sameImage(same, r));
    core::resample(r, 16, 12, core::resampleType::Bilinear);
    CHECK(r.getRows() == 16 && r.getCols() == 12 && r.getChannels() == 3);
    core::resample(r, 5, 5, core::resampleType::Bicubica);
    CHECK(r.getRows() == 5 && r.getCols() == 5 && r.getChannels() == 3);
}

static void testStatsAndMetrics() {
    Image img = allGrayLevels();
    core::ImageStats st = core::computeStats(img);
    CHECK(st.minValue == 0 && st.maxValue == 255 && st.dynamicRange == 255);
    CHECK(st.contrast == 1.0);

    Image c = colorImage();
    core::StorageMetrics sm = core::computeStorageMetrics(c);
    CHECK(sm.bits == 8LL * 8 * 3 * 8);
    core::SpatialMetrics sp = core::computeSpatialMetrics(img);
    CHECK(sp.widthInches > 0.2 && sp.widthInches < 0.23);

    Image big(2, 2);
    big.samples()[0] = -5.0f;
    big.samples()[1] = 300.0f;
    big.clampToRange();
    CHECK(big.at(0, 0) == 0.0f && big.at(0, 1) == 255.0f);
}

int main() {
    testGrayRoundTrip();
    testGrayBmpStaysGray();
    testColorRoundTrip();
    testSaveFormats();
    testStegoInMemory();
    testStegoSurvivesLosslessFiles();
    testIntensity();
    testQuantizeAndResample();
    testStatsAndMetrics();

    if (failures == 0) {
        std::printf("Todos os testes passaram.\n");
        return 0;
    }
    std::printf("%d verificacao(oes) falharam.\n", failures);
    return 1;
}
