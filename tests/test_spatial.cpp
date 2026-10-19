// Testes das operacoes espaciais (escala e rotacao) - secao 6 do plano.
#include "test_common.hpp"
#include "Image.hpp"
#include "ImageWarp.hpp"
#include "ImageScale.hpp"
#include "ImageRotate.hpp"
#include <algorithm>
#include <cmath>
#include <random>

using namespace core;

namespace {

    Image randomImage(int rows, int cols, unsigned seed) {
        std::mt19937 gen(seed);
        std::uniform_int_distribution<int> dist(0, 255);
        Image img(rows, cols);
        for (int y = 0; y < rows; ++y)
            for (int x = 0; x < cols; ++x) img.pixel(y, x) = dist(gen);
        return img;
    }

    Image constantImage(int rows, int cols, int value) {
        Image img(rows, cols);
        img.fill(value);
        return img;
    }

    bool equalImages(Image& a, Image& b) {
        if (a.getRows() != b.getRows() || a.getCols() != b.getCols()) return false;
        return a.getData() == b.getData();
    }


}

// 1. Escala 1,0 e rotacao 0 reproduzem a imagem exatamente (qualquer interpolacao)
TEST(escala_1_reproduz_imagem) {
    Image img = randomImage(37, 53, 1);
    for (Interpolation it : {Interpolation::Nearest, Interpolation::Bilinear}) {
        for (bool aa : {false, true}) {
            Image out = scaleImage(img, 1.0, 1.0, it, aa);
            CHECK(equalImages(img, out));
        }
    }
}

TEST(rotacao_0_reproduz_imagem) {
    Image img = randomImage(41, 29, 2);
    double px, py;
    defaultPivot(img.getRows(), img.getCols(), px, py);
    for (Interpolation it : {Interpolation::Nearest, Interpolation::Bilinear}) {
        for (RotationCanvas cv : {RotationCanvas::KeepSize, RotationCanvas::Expand}) {
            Image out = rotateImage(img, 0.0, px, py, it, cv, 0);
            CHECK(equalImages(img, out));
        }
        // pivo arbitrario tambem nao muda nada em 0 graus
        Image out = rotateImage(img, 0.0, 3.0, 17.0, it, RotationCanvas::KeepSize, 0);
        CHECK(equalImages(img, out));
    }
}

// 2. Escala x2 com vizinho mais proximo duplica linhas e colunas
TEST(escala_2x_vizinho_replica_pixels) {
    Image img = randomImage(9, 13, 3);
    Image out = scaleImage(img, 2.0, 2.0, Interpolation::Nearest, false);
    CHECK(out.getRows() == 18 && out.getCols() == 26);
    bool ok = true;
    for (int y = 0; y < out.getRows(); ++y)
        for (int x = 0; x < out.getCols(); ++x)
            if (out.pixel(y, x) != img.pixel(y / 2, x / 2)) ok = false;
    CHECK(ok);
}

TEST(escala_tamanho_e_eixos_independentes) {
    Image img = randomImage(10, 20, 4);
    Image out = scaleImage(img, 1.5, 0.5, Interpolation::Bilinear, false);
    CHECK(out.getCols() == 30);
    CHECK(out.getRows() == 5);
    int r = 0, c = 0;
    CHECK(computeScaledSize(10, 20, 0.001, 0.001, r, c) && r == 1 && c == 1);   // minimo 1x1
    CHECK(!computeScaledSize(10, 20, -1.0, 1.0, r, c));
    CHECK(!computeScaledSize(10, 20, 0.0, 1.0, r, c));
    CHECK(!computeScaledSize(10000, 10000, 20.0, 20.0, r, c));   // > 100 megapixels
}

// 3. Bilinear em imagem constante devolve imagem constante (sem bordas escuras)
TEST(bilinear_imagem_constante) {
    Image img = constantImage(31, 47, 200);
    for (double s : {0.37, 1.0, 1.73, 3.0}) {
        for (bool aa : {false, true}) {
            Image out = scaleImage(img, s, s, Interpolation::Bilinear, aa);
            bool ok = true;
            for (int y = 0; y < out.getRows(); ++y)
                for (int x = 0; x < out.getCols(); ++x)
                    if (out.pixel(y, x) != 200) ok = false;
            CHECK(ok);
        }
    }
    // Rotacao expandindo: dentro da imagem original o valor e 200; o fundo e 0.
    // Rotacao mantendo tamanho em 0,5 graus: quase toda a imagem deve ser 200.
    double px, py;
    defaultPivot(img.getRows(), img.getCols(), px, py);
    Image rot = rotateImage(img, 0.5, px, py, Interpolation::Bilinear, RotationCanvas::KeepSize, 0);
    CHECK(rot.pixel(15, 23) == 200);
    bool onlyValid = true;
    for (int y = 0; y < rot.getRows(); ++y)
        for (int x = 0; x < rot.getCols(); ++x) {
            int v = rot.pixel(y, x);
            if (v != 200 && v != 0) onlyValid = false;   // nenhum valor intermediario escurecido
        }
    CHECK(onlyValid);
}

// 4. Rotacao 90/180/270 com "expandir" e vizinho mais proximo coincide com np.rot90
TEST(rotacao_multiplos_de_90_expandir) {
    Image img = randomImage(5, 7, 5);
    const int H = img.getRows(), W = img.getCols();
    double px, py;
    defaultPivot(H, W, px, py);

    // 90 graus anti-horarios: out[i][j] = I[j][W-1-i]   (saida W x H)
    Image r90 = rotateImage(img, 90.0, px, py, Interpolation::Nearest, RotationCanvas::Expand, 0);
    CHECK(r90.getRows() == W && r90.getCols() == H);
    bool ok = true;
    for (int i = 0; i < r90.getRows(); ++i)
        for (int j = 0; j < r90.getCols(); ++j)
            if (r90.pixel(i, j) != img.pixel(j, W - 1 - i)) ok = false;
    CHECK(ok);

    // 180 graus: out[i][j] = I[H-1-i][W-1-j]
    Image r180 = rotateImage(img, 180.0, px, py, Interpolation::Nearest, RotationCanvas::Expand, 0);
    CHECK(r180.getRows() == H && r180.getCols() == W);
    ok = true;
    for (int i = 0; i < H; ++i)
        for (int j = 0; j < W; ++j)
            if (r180.pixel(i, j) != img.pixel(H - 1 - i, W - 1 - j)) ok = false;
    CHECK(ok);

    // 270 graus anti-horarios (= 90 horarios): out[i][j] = I[H-1-j][i]   (saida W x H)
    Image r270 = rotateImage(img, 270.0, px, py, Interpolation::Nearest, RotationCanvas::Expand, 0);
    CHECK(r270.getRows() == W && r270.getCols() == H);
    ok = true;
    for (int i = 0; i < r270.getRows(); ++i)
        for (int j = 0; j < r270.getCols(); ++j)
            if (r270.pixel(i, j) != img.pixel(H - 1 - j, i)) ok = false;
    CHECK(ok);

    // -90 e 270 sao a mesma rotacao
    Image rm90 = rotateImage(img, -90.0, px, py, Interpolation::Nearest, RotationCanvas::Expand, 0);
    CHECK(equalImages(r270, rm90));
}

// 5. Rotar theta e depois -theta aproxima a original (miolo)
TEST(rotacao_ida_e_volta) {
    // imagem suave para o erro de interpolacao ser pequeno
    const int H = 120, W = 160;
    Image img(H, W);
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
            img.pixel(y, x) = static_cast<int>(127.5 + 100.0 * std::sin(x / 17.0) * std::cos(y / 23.0));

    double px, py;
    defaultPivot(H, W, px, py);
    Image rot = rotateImage(img, 17.0, px, py, Interpolation::Bilinear, RotationCanvas::Expand, 0);
    double px2, py2;
    defaultPivot(rot.getRows(), rot.getCols(), px2, py2);
    Image back = rotateImage(rot, -17.0, px2, py2, Interpolation::Bilinear, RotationCanvas::Expand, 0);

    // 'back' e maior que a original (expansao dupla): compara o miolo centrado.
    const int offY = (back.getRows() - H) / 2;
    const int offX = (back.getCols() - W) / 2;
    double sumErr = 0.0;
    int maxErr = 0, n = 0;
    const int margin = 8;
    for (int y = margin; y < H - margin; ++y)
        for (int x = margin; x < W - margin; ++x) {
            int e = std::abs(back.pixel(y + offY, x + offX) - img.pixel(y, x));
            sumErr += e; maxErr = std::max(maxErr, e); ++n;
        }
    CHECK(sumErr / n < 3.0);
    CHECK(maxErr < 25);
}

// 6. Rotacao em torno de um pivo (tela original) deixa o pivo fixo
TEST(rotacao_pivo_fica_fixo) {
    Image img = randomImage(60, 80, 6);
    const double pivots[][2] = {{10, 20}, {0, 0}, {79, 59}, {40, 30}, {5, 55}};
    for (auto& pv : pivots) {
        for (double ang : {13.0, 90.0, -47.5, 200.0}) {
            Image out = rotateImage(img, ang, pv[0], pv[1], Interpolation::Nearest, RotationCanvas::KeepSize, 0);
            CHECK(out.pixel(static_cast<int>(pv[1]), static_cast<int>(pv[0]))
                  == img.pixel(static_cast<int>(pv[1]), static_cast<int>(pv[0])));
        }
    }
}

TEST(rotacao_pivo_muda_em_keep_e_nao_em_expand) {
    Image img = randomImage(40, 50, 7);
    double cx, cy;
    defaultPivot(img.getRows(), img.getCols(), cx, cy);

    Image keepA = rotateImage(img, 30.0, cx, cy, Interpolation::Bilinear, RotationCanvas::KeepSize, 0);
    Image keepB = rotateImage(img, 30.0, 5.0, 5.0, Interpolation::Bilinear, RotationCanvas::KeepSize, 0);
    CHECK(!equalImages(keepA, keepB));            // tela original: o pivo importa

    Image expA = rotateImage(img, 30.0, cx, cy, Interpolation::Bilinear, RotationCanvas::Expand, 0);
    Image expB = rotateImage(img, 30.0, 5.0, 5.0, Interpolation::Bilinear, RotationCanvas::Expand, 0);
    CHECK(equalImages(expA, expB));               // expandir: nao importa
}

TEST(rotacao_sentido_anti_horario) {
    // Um ponto brilhante a direita do centro, rotacionado 90 graus anti-horarios,
    // deve ficar acima do centro (y menor).
    Image img = constantImage(21, 21, 0);
    img.pixel(10, 16) = 255;   // linha 10 (centro), coluna 16 (direita)
    Image out = rotateImage(img, 90.0, 10.0, 10.0, Interpolation::Nearest, RotationCanvas::KeepSize, 0);
    CHECK(out.pixel(4, 10) == 255);
}

// 7. Bilinear comparado com uma referencia independente
//    (duas interpolacoes horizontais + uma vertical), tolerancia <= 1 nivel.
//    A mesma comparacao foi feita contra scipy.ndimage.map_coordinates(order=1)
//    fora deste executavel (ver README).
TEST(bilinear_contra_referencia_independente) {
    Image img = randomImage(50, 64, 8);
    const int H = img.getRows(), W = img.getCols();
    std::mt19937 gen(99);
    std::uniform_real_distribution<double> ux(0.0, W - 1.0), uy(0.0, H - 1.0);
    int maxDiff = 0;
    for (int i = 0; i < 20000; ++i) {
        const double xs = ux(gen), ys = uy(gen);
        const int x0 = static_cast<int>(std::floor(xs)), y0 = static_cast<int>(std::floor(ys));
        const int x1 = std::min(x0 + 1, W - 1), y1 = std::min(y0 + 1, H - 1);
        const double fx = xs - x0, fy = ys - y0;
        const double top = img.pixel(y0, x0) * (1 - fx) + img.pixel(y0, x1) * fx;
        const double bot = img.pixel(y1, x0) * (1 - fx) + img.pixel(y1, x1) * fx;
        const double ref = top * (1 - fy) + bot * fy;
        const double got = sampleBilinear(img, xs, ys);
        maxDiff = std::max(maxDiff, static_cast<int>(std::lround(std::fabs(ref - got))));
        CHECK_NEAR(got, ref, 1e-9);
    }
    CHECK(maxDiff <= 1);
}

TEST(bilinear_bordas_nao_escurecem) {
    // Ponto na borda do dominio valido (-0.5 e W-0.5): usa clamp dos vizinhos.
    Image img = constantImage(10, 10, 100);
    CHECK_NEAR(sampleBilinear(img, -0.5, -0.5), 100.0, 1e-9);
    CHECK_NEAR(sampleBilinear(img, 9.5, 9.5), 100.0, 1e-9);
    CHECK_NEAR(sampleBilinear(img, 9.5, -0.5), 100.0, 1e-9);
    CHECK(sampleNearest(img, 9.5, 9.5) == 100.0);
}

TEST(antialias_reduz_aliasing) {
    // Listras de 1 pixel (0/255) reduzidas a 1/4: sem suavizar, o vizinho mais
    // proximo escolhe so um tom (aliasing forte); com a caixa, o resultado se
    // aproxima da media (~127).
    Image img(64, 64);
    for (int y = 0; y < 64; ++y)
        for (int x = 0; x < 64; ++x) img.pixel(y, x) = (x % 2) ? 255 : 0;
    Image raw = scaleImage(img, 0.25, 0.25, Interpolation::Nearest, false);
    Image smooth = scaleImage(img, 0.25, 0.25, Interpolation::Nearest, true);
    double devRaw = 0, devSmooth = 0;
    for (int y = 0; y < raw.getRows(); ++y)
        for (int x = 0; x < raw.getCols(); ++x) {
            devRaw += std::fabs(raw.pixel(y, x) - 127.5);
            devSmooth += std::fabs(smooth.pixel(y, x) - 127.5);
        }
    CHECK(devSmooth < devRaw * 0.25);
}

TEST(boxblur_constante_e_media) {
    Image img = constantImage(20, 30, 77);
    Image b = boxBlur(img, 5, 7);
    CHECK(equalImages(img, b));
    // media de 3 pixels em linha com borda replicada
    Image line(1, 5);
    int vals[5] = {0, 30, 60, 90, 120};
    for (int i = 0; i < 5; ++i) line.pixel(0, i) = vals[i];
    Image bl = boxBlur(line, 1, 3);
    CHECK(bl.pixel(0, 0) == 10);    // (0+0+30)/3
    CHECK(bl.pixel(0, 2) == 60);    // (30+60+90)/3
    CHECK(bl.pixel(0, 4) == 110);   // (90+120+120)/3
}

TEST(warp_afim_composicao) {
    // (A*B) aplica B primeiro; inversa de uma afim
    Affine2D a = Affine2D::translation(3, -2) * Affine2D::scale(2, 4);
    double x, y;
    a.apply(1.0, 1.0, x, y);
    CHECK_NEAR(x, 5.0, 1e-12);   // 1*2 + 3
    CHECK_NEAR(y, 2.0, 1e-12);   // 1*4 - 2
    Affine2D inv;
    CHECK(a.inverse(inv));
    double bx, by;
    inv.apply(x, y, bx, by);
    CHECK_NEAR(bx, 1.0, 1e-12);
    CHECK_NEAR(by, 1.0, 1e-12);
    Affine2D sing = Affine2D::scale(0.0, 1.0);
    Affine2D dummy;
    CHECK(!sing.inverse(dummy));
}

TEST(rotacao_tamanho_expandido_e_limite) {
    RotationLayout lay;
    CHECK(computeRotationLayout(100, 200, 90.0, 99.5, 49.5, RotationCanvas::Expand, lay));
    CHECK(lay.outRows == 200 && lay.outCols == 100);
    CHECK(computeRotationLayout(100, 200, 45.0, 99.5, 49.5, RotationCanvas::Expand, lay));
    // diagonal: (200+100)/sqrt2 ~ 212.13 => 213
    CHECK(lay.outCols == 213 && lay.outRows == 213);
    CHECK(!computeRotationLayout(20000, 20000, 0.0, 0.0, 0.0, RotationCanvas::KeepSize, lay));   // 400 MP
}

int main() { return testing::runAll(); }
