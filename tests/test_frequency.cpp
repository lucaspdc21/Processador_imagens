// Testes da filtragem no dominio da frequencia - secao 6 do plano.
#include "test_common.hpp"
#include "Image.hpp"
#include "FrequencyTransform.hpp"
#include "FrequencyFilters.hpp"
#include "FrequencyMask.hpp"
#include "FrequencySession.hpp"
#include <algorithm>
#include <cmath>
#include <random>

using namespace core;

namespace {

    const double kPi = 3.14159265358979323846;

    ComplexMatrix randomMatrix(int r, int c, unsigned seed) {
        std::mt19937 g(seed);
        std::uniform_real_distribution<double> d(0.0, 255.0);
        ComplexMatrix m(r, c);
        for (auto& v : m.data) v = Complex(std::floor(d(g)), 0.0);
        return m;
    }

    // Oraculo independente: Eq. 4.5-15 com os 4 lacos (O((MN)^2)), tamanhos pequenos.
    ComplexMatrix bruteForceDft2d(const ComplexMatrix& f) {
        const int M = f.rows, N = f.cols;
        ComplexMatrix F(M, N);
        for (int u = 0; u < M; ++u)
            for (int v = 0; v < N; ++v) {
                Complex s(0, 0);
                for (int x = 0; x < M; ++x)
                    for (int y = 0; y < N; ++y) {
                        const double ang = -2.0 * kPi * (static_cast<double>(u) * x / M + static_cast<double>(v) * y / N);
                        s += f.at(x, y) * Complex(std::cos(ang), std::sin(ang));
                    }
                F.at(u, v) = s;
            }
        return F;
    }

    // Oraculo 1-D ingenuo (so para os testes): F(u) = sum f(x) e^{-j2pi ux/M}
    std::vector<Complex> naiveDft1d(const std::vector<Complex>& f) {
        const int n = static_cast<int>(f.size());
        std::vector<Complex> F(f.size());
        for (int u = 0; u < n; ++u) {
            Complex s(0, 0);
            for (int x = 0; x < n; ++x) {
                const double ang = -2.0 * kPi * static_cast<double>(u) * x / n;
                s += f[static_cast<size_t>(x)] * Complex(std::cos(ang), std::sin(ang));
            }
            F[static_cast<size_t>(u)] = s;
        }
        return F;
    }

    double relError(const ComplexMatrix& a, const ComplexMatrix& b) {
        double num = 0, den = 0;
        for (size_t i = 0; i < a.data.size(); ++i) {
            num += std::norm(a.data[i] - b.data[i]);
            den += std::norm(b.data[i]);
        }
        return std::sqrt(num / std::max(den, 1e-300));
    }

    Image randomImage(int rows, int cols, unsigned seed) {
        std::mt19937 g(seed);
        Image img(rows, cols);
        for (int y = 0; y < rows; ++y)
            for (int x = 0; x < cols; ++x) img.pixel(y, x) = static_cast<int>(g() % 256);
        return img;
    }

    // Imagem de teste "natural": gradiente suave + bloco + ruido leve.
    Image testScene(int rows, int cols) {
        Image img(rows, cols);
        std::mt19937 g(5);
        for (int y = 0; y < rows; ++y)
            for (int x = 0; x < cols; ++x) {
                double v = 80.0 + 60.0 * std::sin(x / 9.0) + 40.0 * std::cos(y / 7.0);
                if (x > cols / 3 && x < 2 * cols / 3 && y > rows / 3 && y < 2 * rows / 3) v += 60.0;
                v += static_cast<double>(g() % 21) - 10.0;
                img.pixel(y, x) = static_cast<int>(std::min(255.0, std::max(0.0, v)));
            }
        return img;
    }

}

// 1. Exemplos do livro (Exemplo 4.4) com a FFT propria
TEST(fft_1d_exemplos_do_livro) {
    auto F = fft1d({1, 2, 4, 4});
    CHECK_NEAR(F[0].real(), 11, 1e-12);  CHECK_NEAR(F[0].imag(), 0, 1e-12);
    CHECK_NEAR(F[1].real(), -3, 1e-12);  CHECK_NEAR(F[1].imag(), 2, 1e-12);
    CHECK_NEAR(F[2].real(), -1, 1e-12);  CHECK_NEAR(F[2].imag(), 0, 1e-12);
    CHECK_NEAR(F[3].real(), -3, 1e-12);  CHECK_NEAR(F[3].imag(), -2, 1e-12);

    auto G = fft1d({1, 2, 3, 4});
    CHECK_NEAR(G[0].real(), 10, 1e-12);
    CHECK_NEAR(G[1].real(), -2, 1e-12);  CHECK_NEAR(G[1].imag(), 2, 1e-12);
    CHECK_NEAR(G[2].real(), -2, 1e-12);
    CHECK_NEAR(G[3].real(), -2, 1e-12);  CHECK_NEAR(G[3].imag(), -2, 1e-12);

    CHECK(fft1d({1, 2, 3}).empty());     // nao e potencia de 2
}

TEST(fft_1d_contra_oraculo) {
    for (int n : {1, 2, 8, 64}) {
        std::vector<Complex> f(static_cast<size_t>(n));
        for (int i = 0; i < n; ++i) f[static_cast<size_t>(i)] = Complex(std::sin(i * 1.3) * 50, std::cos(i * 0.7) * 20);
        auto a = fft1d(f), b = naiveDft1d(f);
        for (int i = 0; i < n; ++i) CHECK_NEAR(std::abs(a[static_cast<size_t>(i)] - b[static_cast<size_t>(i)]), 0.0, 1e-9);
    }
}

// 2. FFT 2-D contra o oraculo de forca bruta (potencias de 2, nao quadrados)
TEST(fft_2d_contra_oraculo) {
    const int sizes[][2] = {{8, 8}, {4, 16}, {16, 4}, {1, 8}, {8, 1}, {32, 8}};
    for (auto& s : sizes) {
        ComplexMatrix f = randomMatrix(s[0], s[1], 100 + s[0] * 31 + s[1]);
        ComplexMatrix ref = bruteForceDft2d(f);
        ComplexMatrix m = f;
        CHECK(forwardTransform2D(m));
        CHECK(relError(m, ref) < 1e-9);
    }
}

TEST(radix2_recusa_tamanho_invalido) {
    ComplexMatrix m = randomMatrix(6, 8, 1);
    CHECK(!forwardTransform2D(m));
}

// 3. IFFT(FFT(f)) == f
TEST(inversa_recupera_a_imagem) {
    const int sizes[][2] = {{16, 16}, {8, 32}, {2, 64}};
    for (auto& s : sizes) {
        ComplexMatrix f = randomMatrix(s[0], s[1], 3);
        ComplexMatrix m = f;
        CHECK(forwardTransform2D(m));
        CHECK(inverseTransform2D(m));
        CHECK(relError(m, f) < 1e-9);
    }
}

// 4. F(0,0) = M N media(f)
TEST(termo_dc) {
    ComplexMatrix f = randomMatrix(16, 32, 4);
    double sum = 0;
    for (auto& v : f.data) sum += v.real();
    const double mean = sum / f.data.size();
    ComplexMatrix m = f;
    forwardTransform2D(m);
    CHECK_NEAR(m.at(0, 0).real(), 16.0 * 32.0 * mean, 1e-8);
    CHECK_NEAR(m.at(0, 0).imag(), 0.0, 1e-8);
}

// 5. Simetria conjugada
TEST(simetria_conjugada) {
    ComplexMatrix m = randomMatrix(16, 32, 5);
    forwardTransform2D(m);
    double maxErr = 0;
    for (int u = 0; u < 16; ++u)
        for (int v = 0; v < 32; ++v)
            maxErr = std::max(maxErr, std::abs(m.at((16 - u) % 16, (32 - v) % 32) - std::conj(m.at(u, v))));
    CHECK(maxErr < 1e-8);
}

// 6. Translacao circular nao altera |F|
TEST(translacao_nao_altera_magnitude) {
    ComplexMatrix f = randomMatrix(16, 16, 6);
    ComplexMatrix g(16, 16);
    for (int x = 0; x < 16; ++x)
        for (int y = 0; y < 16; ++y) g.at((x + 3) % 16, (y + 5) % 16) = f.at(x, y);
    forwardTransform2D(f);
    forwardTransform2D(g);
    double maxErr = 0;
    for (size_t i = 0; i < f.data.size(); ++i)
        maxErr = std::max(maxErr, std::fabs(std::abs(f.data[i]) - std::abs(g.data[i])));
    CHECK(maxErr < 1e-8);
}

// Dimensoes P x Q: sem preenchimento (so arredonda para a proxima potencia de 2)
TEST(tamanhos_sem_preenchimento) {
    TransformSize s = computeTransformSize(720, 1280);
    CHECK(s.P == 1024 && s.Q == 2048);
    s = computeTransformSize(30, 50);
    CHECK(s.P == 32 && s.Q == 64);
    s = computeTransformSize(32, 64);            // ja potencias de 2: P = M, Q = N
    CHECK(s.P == 32 && s.Q == 64);
    s = computeTransformSize(1, 1);
    CHECK(s.P == 2 && s.Q == 2);
}

// 7. H = 1 devolve a imagem de entrada (tamanhos potencia de 2 e nao potencia de 2)
TEST(filtro_identidade_devolve_a_imagem) {
    const int sizes[][2] = {{21, 30}, {32, 64}, {17, 5}};
    for (auto& sz : sizes) {
        Image img = randomImage(sz[0], sz[1], 8);
        FrequencySession s;
        CHECK(s.compute(img));
        CHECK(s.filter());
        double maxErr = 0;
        for (int x = 0; x < sz[0]; ++x)
            for (int y = 0; y < sz[1]; ++y)
                maxErr = std::max(maxErr, std::fabs(s.resultRaw()[static_cast<size_t>(x) * sz[1] + y] - img.pixel(x, y)));
        CHECK(maxErr < 1e-8);
        Image out = s.resultImage(OutputNormalization::Clip);
        CHECK(out.getRows() == sz[0] && out.getCols() == sz[1]);
        CHECK(out.getData() == img.getData());
    }
}

// 8. Pincelada simetrica: H exatamente simetrica e residuo imaginario ~ 0
TEST(pincelada_simetrica_mantem_resultado_real) {
    Image img = testScene(30, 40);
    FrequencySession s;
    CHECK(s.compute(img));
    const int P = s.P(), Q = s.Q();

    Brush b;
    b.radius = 6; b.hardness = 0.3; b.opacity = 0.8; b.value = 0.0;
    s.mask().beginStroke();
    s.mask().paintSegment(P / 2 - 10, Q / 2 + 12, P / 2 - 3.5, Q / 2 + 40.25, b);        // perto do centro
    s.mask().paintSegment(2.3, 5.7, 0.0, 0.0, b);                                        // borda (u=0, v=0: auto-conjugados)
    s.mask().paintSegment(0.0, Q / 2.0, 1.0, Q / 2.0 + 3.0, b);
    b.value = 0.5; b.hardDisc = true; b.radius = 4;
    s.mask().paintSegment(P - 5.0, Q - 7.0, P - 1.0, Q - 1.0, b);
    s.mask().endStroke();

    bool exact = true;
    bool changed = false;
    for (int u = 0; u < P; ++u)
        for (int v = 0; v < Q; ++v) {
            if (s.mask().at(u, v) != s.mask().at((P - u) % P, (Q - v) % Q)) exact = false;
            if (s.mask().at(u, v) != 1.0f) changed = true;
        }
    CHECK(exact);
    CHECK(changed);

    CHECK(s.filter());
    CHECK(s.residualImag() < 1e-8);
}

TEST(pincel_preto_remove_e_branco_restaura) {
    SpectrumMask m;
    m.reset(64, 64);
    Brush b; b.radius = 5; b.hardDisc = true; b.opacity = 1.0; b.value = 0.0;
    m.beginStroke();
    m.paintSegment(32, 32, 32, 32, b);
    m.endStroke();
    CHECK(m.at(32, 32) == 0.0f);
    CHECK(m.at(32, 40) == 1.0f);                 // fora do raio
    CHECK(m.at(32, 32 + 5) == 0.0f);             // dentro (d = 5 <= raio)
    CHECK(m.at(32, 32 - 5) == 0.0f);             // espelho do centro e ele mesmo (32,32)

    b.value = 1.0;                                // branco restaura
    m.beginStroke();
    m.paintSegment(32, 32, 32, 32, b);
    m.endStroke();
    CHECK(m.at(32, 32) == 1.0f);

    // cinza com opacidade parcial: H <- H(1-a*op) + v*a*op
    b.value = 0.5; b.opacity = 0.5;
    m.beginStroke();
    m.paintSegment(10, 20, 10, 20, b);
    m.endStroke();
    CHECK_NEAR(m.at(10, 20), 0.75, 1e-6);        // 1*(1-0.5) + 0.5*0.5
}

TEST(pincel_suave_decai_gradualmente) {
    Brush b; b.radius = 10; b.hardness = 0.0;
    double prev = 2.0;
    for (int d = 0; d <= 30; ++d) {
        const double a = SpectrumMask::brushAlpha(d, b);
        CHECK(a <= prev + 1e-12);
        prev = a;
    }
    CHECK_NEAR(SpectrumMask::brushAlpha(0, b), 1.0, 1e-12);
    CHECK_NEAR(SpectrumMask::brushAlpha(10, b), std::exp(-0.5), 1e-12);   // sigma = raio (dureza 0): formula do plano
    Brush hard; hard.radius = 10; hard.hardness = 1.0;
    CHECK(SpectrumMask::brushAlpha(6, hard) > 0.999);                     // dureza 1: quase disco
    CHECK(SpectrumMask::brushAlpha(14, hard) < 0.01);
}

TEST(desfazer_refazer_restauram_a_mascara) {
    SpectrumMask m;
    m.reset(32, 32);
    const std::vector<float> original = m.values();
    Brush b; b.radius = 4; b.hardDisc = true;
    m.beginStroke(); m.paintSegment(5, 5, 20, 22, b); m.endStroke();
    const std::vector<float> afterStroke = m.values();
    CHECK(afterStroke != original);
    m.invert();
    const std::vector<float> afterInvert = m.values();
    CHECK(m.canUndo());

    m.undo();
    CHECK(m.values() == afterStroke);
    m.undo();
    CHECK(m.values() == original);
    CHECK(!m.canUndo());
    CHECK(m.canRedo());
    m.redo();
    CHECK(m.values() == afterStroke);
    m.redo();
    CHECK(m.values() == afterInvert);

    m.clear();
    for (float v : m.values()) CHECK(v == 1.0f);
    m.undo();
    CHECK(m.values() == afterInvert);
}

// 9. Passa-baixa gaussiano borra; passa-alta realca bordas
TEST(passa_baixa_borra_e_passa_alta_realca) {
    Image img = testScene(64, 64);

    auto energyHighFreq = [](const std::vector<double>& g, int M, int N) {
        // energia de alta frequencia ~ soma dos quadrados das diferencas entre vizinhos
        double e = 0;
        for (int x = 0; x + 1 < M; ++x)
            for (int y = 0; y + 1 < N; ++y) {
                const double a = g[static_cast<size_t>(x) * N + y];
                e += std::pow(a - g[static_cast<size_t>(x + 1) * N + y], 2) + std::pow(a - g[static_cast<size_t>(x) * N + y + 1], 2);
            }
        return e;
    };
    std::vector<double> orig(64 * 64);
    for (int x = 0; x < 64; ++x) for (int y = 0; y < 64; ++y) orig[static_cast<size_t>(x) * 64 + y] = img.pixel(x, y);
    const double e0 = energyHighFreq(orig, 64, 64);

    double eLow = 0, eHigh = 0;
    for (FilterKind k : {FilterKind::LowPass, FilterKind::HighPass}) {
        FrequencySession s;
        CHECK(s.compute(img));
        FilterParams p; p.kind = k; p.shape = FilterShape::Gaussian; p.d0 = 6.0;
        s.mask().multiplyBy(makeFilter(s.P(), s.Q(), p));
        CHECK(s.filter());
        const double e = energyHighFreq(s.resultRaw(), 64, 64);
        (k == FilterKind::LowPass ? eLow : eHigh) = e;
    }
    CHECK(eLow < 0.30 * e0);     // passa-baixa: borrou (perdeu energia de alta frequencia)
    CHECK(eHigh > 0.80 * e0);    // passa-alta: mantem bordas/ruido, remove so o fundo suave
    CHECK(eHigh > 2.0 * eLow);
}

TEST(filtros_predefinidos_formulas) {
    const int P = 64, Q = 64;
    FilterParams p;
    p.d0 = 10; p.order = 2; p.width = 6;

    // Ideal LP: 1 dentro, 0 fora, centro em (P/2, Q/2)
    p.kind = FilterKind::LowPass; p.shape = FilterShape::Ideal;
    auto h = makeFilter(P, Q, p);
    CHECK(h[32 * Q + 32] == 1.0f);
    CHECK(h[32 * Q + 42] == 1.0f);        // D = 10 <= D0
    CHECK(h[32 * Q + 43] == 0.0f);

    // Butterworth LP em D = D0 vale 1/2
    p.shape = FilterShape::Butterworth;
    h = makeFilter(P, Q, p);
    CHECK_NEAR(h[32 * Q + 42], 0.5, 1e-6);

    // Gaussiano LP em D = D0 vale exp(-1/2)
    p.shape = FilterShape::Gaussian;
    h = makeFilter(P, Q, p);
    CHECK_NEAR(h[32 * Q + 42], std::exp(-0.5), 1e-6);

    // HP = 1 - LP
    auto lp = h;
    p.kind = FilterKind::HighPass;
    auto hp = makeFilter(P, Q, p);
    for (size_t i = 0; i < lp.size(); ++i) CHECK_NEAR(lp[i] + hp[i], 1.0, 1e-6);

    // Rejeita-banda ideal: 0 na banda [D0-W/2, D0+W/2]; passa-banda = 1 - BR
    p.kind = FilterKind::BandReject; p.shape = FilterShape::Ideal;
    auto br = makeFilter(P, Q, p);
    CHECK(br[32 * Q + 42] == 0.0f);       // D = 10
    CHECK(br[32 * Q + 32] == 1.0f);       // D = 0
    CHECK(br[32 * Q + 50] == 1.0f);       // D = 18
    p.kind = FilterKind::BandPass;
    auto bp = makeFilter(P, Q, p);
    for (size_t i = 0; i < br.size(); ++i) CHECK_NEAR(br[i] + bp[i], 1.0, 1e-6);

    // Rejeita-banda Butterworth: 0 em D = D0, ~1 longe; Gaussiano idem
    p.kind = FilterKind::BandReject; p.shape = FilterShape::Butterworth;
    auto brb = makeFilter(P, Q, p);
    CHECK_NEAR(brb[32 * Q + 42], 0.0, 1e-6);
    CHECK(brb[32 * Q + 32] > 0.999);
    p.shape = FilterShape::Gaussian;
    auto brg = makeFilter(P, Q, p);
    CHECK_NEAR(brg[32 * Q + 42], 0.0, 1e-6);
    CHECK(brg[32 * Q + 32] == 1.0f);

    // Notch Butterworth: zera o centro do notch e o simetrico, deixa o resto
    p.kind = FilterKind::NotchReject; p.shape = FilterShape::Butterworth;
    p.notches = {Notch{5.0, 12.0, 4.0}};
    auto nr = makeFilter(P, Q, p);
    CHECK_NEAR(nr[(32 + 5) * Q + (32 + 12)], 0.0, 1e-6);
    CHECK_NEAR(nr[(32 - 5) * Q + (32 - 12)], 0.0, 1e-6);
    CHECK(nr[10 * Q + 10] > 0.99);
    p.kind = FilterKind::NotchPass;
    auto np = makeFilter(P, Q, p);
    for (size_t i = 0; i < nr.size(); ++i) CHECK_NEAR(nr[i] + np[i], 1.0, 1e-6);
    // simetria da mascara de notch
    bool sym = true;
    for (int u = 1; u < P; ++u) for (int v = 1; v < Q; ++v)
        if (std::fabs(nr[u * Q + v] - nr[(P - u) * Q + (Q - v)]) > 1e-6) sym = false;
    CHECK(sym);
}

TEST(notch_remove_padrao_periodico_moire) {
    // Imagem = cena + senoide forte; um notch na frequencia da senoide a remove.
    const int M = 64, N = 64;
    Image clean = testScene(M, N);
    Image noisy(M, N);
    for (int x = 0; x < M; ++x)
        for (int y = 0; y < N; ++y)
            noisy.pixel(x, y) = clean.pixel(x, y) + static_cast<int>(std::lround(40.0 * std::sin(2 * kPi * (6.0 * x / M + 10.0 * y / N))));

    auto mse = [&](Image& a, const std::vector<double>& g) {
        double e = 0;
        for (int x = 0; x < M; ++x) for (int y = 0; y < N; ++y) e += std::pow(a.pixel(x, y) - g[static_cast<size_t>(x) * N + y], 2);
        return e / (M * N);
    };
    std::vector<double> noisyRaw(M * N);
    for (int x = 0; x < M; ++x) for (int y = 0; y < N; ++y) noisyRaw[static_cast<size_t>(x) * N + y] = noisy.pixel(x, y);
    const double before = mse(clean, noisyRaw);

    FrequencySession s;
    CHECK(s.compute(noisy));   // P = Q = 64: a senoide cai exatamente em (6,10)
    const double scale = s.P() / static_cast<double>(M);
    s.mask().multiplyBy(makeNotchRejectAt(s.P(), s.Q(), s.P() / 2.0 + 6 * scale, s.Q() / 2.0 + 10 * scale, 2.5, 2, FilterShape::Gaussian));
    CHECK(s.filter());
    const double after = mse(clean, s.resultRaw());
    CHECK(after < 0.1 * before);
}

TEST(sessao_validacoes) {
    Image empty(0, 0);
    FrequencySession s;
    CHECK(!s.compute(empty));
    CHECK(!s.filter());                       // sem FFT calculada
    Image img = randomImage(10, 10, 1);
    CHECK(s.compute(img));
    CHECK(s.P() == 16 && s.Q() == 16);
    // cancelamento
    TransformControl ctl;
    ctl.cancel = true;
    FrequencySession s2;
    CHECK(!s2.compute(img, &ctl));
    CHECK(!s2.hasTransform());
}

TEST(espectro_normalizacao_e_exibicao) {
    Image img = testScene(16, 16);
    FrequencySession s;
    CHECK(s.compute(img));     // 16 x 16 ja e potencia de 2: P = M, Q = N
    // o termo DC (centro) e o maximo: 255 no espectro original
    s.setDisplayMode(DisplayMode::OriginalSpectrum);
    s.renderAll();
    const int c = s.P() / 2, d = s.Q() / 2;
    CHECK(s.displayRGBA()[(static_cast<size_t>(c) * s.Q() + d) * 4] == 255);
    // zerar o DC pela mascara deixa o centro preto no espectro filtrado
    Brush b; b.hardDisc = true; b.radius = 0.4; b.value = 0.0;
    s.mask().beginStroke(); s.mask().paintSegment(c, d, c, d, b); s.mask().endStroke();
    s.setDisplayMode(DisplayMode::FilteredSpectrum);
    s.renderAll();
    CHECK(s.displayRGBA()[(static_cast<size_t>(c) * s.Q() + d) * 4] == 0);
    CHECK(s.filter());
    // F(0,0)=0 => media nula; resultado normalizado min-max fica em [0, 255]
    double mean = 0; for (double v : s.resultRaw()) mean += v; mean /= s.resultRaw().size();
    CHECK(std::fabs(mean) < 1e-8);
    Image out = s.resultImage(OutputNormalization::MinMax);
    int mn = 255, mx = 0;
    for (int x = 0; x < 16; ++x) for (int y = 0; y < 16; ++y) { mn = std::min(mn, out.pixel(x, y)); mx = std::max(mx, out.pixel(x, y)); }
    CHECK(mn == 0 && mx == 255);
}

int main() { return testing::runAll(); }
