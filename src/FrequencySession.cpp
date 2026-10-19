#include "FrequencySession.hpp"
#include <algorithm>
#include <cmath>

namespace core {

namespace {
    constexpr double kPi = 3.14159265358979323846;

    inline double signXY(int x, int y) { return ((x + y) & 1) ? -1.0 : 1.0; }   // (-1)^(x+y)

    inline uint8_t toByte(double v) {
        if (!(v > 0.0)) return 0;
        if (v >= 255.0) return 255;
        return static_cast<uint8_t>(v + 0.5);
    }
}

PaddedSize computePaddedSize(int M, int N, TransformAlgorithm algo, bool usePadding) {
    PaddedSize s;
    if (M <= 0 || N <= 0) return s;
    if (algo == TransformAlgorithm::RadixFFT) {
        // potencia de 2 (>= 2 para ser par)
        const int pm = usePadding ? 2 * M - 1 : M;
        const int pn = usePadding ? 2 * N - 1 : N;
        s.P = nextPowerOfTwo(std::max(2, pm));
        s.Q = nextPowerOfTwo(std::max(2, pn));
    } else if (usePadding) {
        s.P = 2 * M;
        s.Q = 2 * N;
    } else {
        s.P = M + (M & 1);
        s.Q = N + (N & 1);
    }
    return s;
}

bool FrequencySession::compute(Image& img, TransformAlgorithm algo, bool usePadding, TransformControl* control) {
    hasF_ = false;
    hasResult_ = false;
    error_.clear();

    const int M = img.getRows();
    const int N = img.getCols();
    if (M <= 0 || N <= 0) {
        error_ = "Imagem vazia.";
        return false;
    }
    if (algo == TransformAlgorithm::LibraryFFT && !libraryFFTAvailable()) {
        error_ = "A FFT da biblioteca não foi compilada neste programa (pocketfft ausente).";
        return false;
    }
    const PaddedSize ps = computePaddedSize(M, N, algo, usePadding);
    if (static_cast<long long>(ps.P) * ps.Q > kMaxPaddedPixels) {
        error_ = "Imagem grande demais para a transformada (limite: 4096 x 4096 após o preenchimento). "
                 "Reduza a imagem (escala) ou desmarque o preenchimento.";
        return false;
    }

    M_ = M; N_ = N; P_ = ps.P; Q_ = ps.Q;
    maxIntensity_ = img.getMaxIntensity();
    bits_ = img.getBitsPerPixel();
    dpi_ = img.getDpi();
    algo_ = algo;

    // Passos 1-2: f_p (P x Q, zeros) com a imagem no canto superior esquerdo,
    // multiplicada por (-1)^(x+y) (centraliza a transformada: Eq. 4.6-8).
    F_ = ComplexMatrix(P_, Q_);
    const auto& data = img.getData();
    for (int x = 0; x < M; ++x) {
        const std::vector<int>& row = data[static_cast<size_t>(x)];
        for (int y = 0; y < N; ++y) {
            F_.at(x, y) = Complex(static_cast<double>(row[static_cast<size_t>(y)]) * signXY(x, y), 0.0);
        }
    }

    // Passo 3: DFT 2-D (uma unica vez; fica em cache).
    if (!forwardTransform2D(F_, algo, control)) {
        F_ = ComplexMatrix();
        error_ = (control && control->cancel.load()) ? "Operação cancelada."
                                                      : "O algoritmo escolhido não consegue tratar este tamanho.";
        return false;
    }

    // S_max = max log(1 + |F|)
    std::vector<double> rowMax(static_cast<size_t>(P_), 0.0);
    parallelForEach(P_, [&](int r) {
        double m = 0.0;
        const Complex* row = &F_.data[static_cast<size_t>(r) * Q_];
        for (int c = 0; c < Q_; ++c) m = std::max(m, std::log1p(std::abs(row[c])));
        rowMax[static_cast<size_t>(r)] = m;
    });
    logMax_ = std::max(1e-12, *std::max_element(rowMax.begin(), rowMax.end()));

    mask_.reset(P_, Q_);
    rgba_.assign(static_cast<size_t>(P_) * Q_ * 4, 255);
    result_.clear();
    hasF_ = true;
    renderAll();
    return true;
}

bool FrequencySession::filter(TransformAlgorithm algo, TransformControl* control) {
    hasResult_ = false;
    error_.clear();
    if (!hasF_) {
        error_ = "Calcule a DFT antes de aplicar.";
        return false;
    }
    if (algo == TransformAlgorithm::LibraryFFT && !libraryFFTAvailable()) {
        error_ = "A FFT da biblioteca não foi compilada neste programa.";
        return false;
    }
    if (algo == TransformAlgorithm::RadixFFT && (!isPowerOfTwo(P_) || !isPowerOfTwo(Q_))) {
        error_ = "A FFT radix-2 exige P e Q potências de 2: recalcule a DFT com esse algoritmo.";
        return false;
    }

    // Passo 5: G(u,v) = H(u,v) F(u,v) (produto elemento a elemento; preserva a fase).
    ComplexMatrix G = F_;
    const std::vector<float>& H = mask_.values();
    parallelForEach(P_, [&](int r) {
        Complex* row = &G.data[static_cast<size_t>(r) * Q_];
        const float* h = &H[static_cast<size_t>(r) * Q_];
        for (int c = 0; c < Q_; ++c) row[c] *= static_cast<double>(h[c]);
    });

    // Passo 6: IDFT (um unico algoritmo direto, Secao 4.11.2).
    if (!inverseTransform2D(G, algo, control)) {
        error_ = (control && control->cancel.load()) ? "Operação cancelada."
                                                      : "Falha ao calcular a transformada inversa.";
        return false;
    }

    // Parte real x (-1)^(x+y), recorte [0:M, 0:N] (passos 6-7). Mede o
    // residuo imaginario na matriz inteira.
    double maxRe = 0.0, maxIm = 0.0;
    for (const Complex& v : G.data) {
        maxRe = std::max(maxRe, std::fabs(v.real()));
        maxIm = std::max(maxIm, std::fabs(v.imag()));
    }
    residualImag_ = maxIm / std::max(maxRe, 1e-300);

    result_.assign(static_cast<size_t>(M_) * N_, 0.0);
    for (int x = 0; x < M_; ++x)
        for (int y = 0; y < N_; ++y)
            result_[static_cast<size_t>(x) * N_ + y] = G.at(x, y).real() * signXY(x, y);

    hasResult_ = true;
    return true;
}

void FrequencySession::setDisplayMode(DisplayMode mode) {
    mode_ = mode;
}

void FrequencySession::renderPixelRow(int r, int c0, int c1) {
    uint8_t* out = &rgba_[(static_cast<size_t>(r) * Q_ + c0) * 4];
    const Complex* f = &F_.data[static_cast<size_t>(r) * Q_];
    const float* h = &mask_.values()[static_cast<size_t>(r) * Q_];
    for (int c = c0; c < c1; ++c, out += 4) {
        double v = 0.0;
        switch (mode_) {
            case DisplayMode::FilteredSpectrum:
                v = 255.0 * std::log1p(static_cast<double>(h[c]) * std::abs(f[c])) / logMax_;
                break;
            case DisplayMode::OriginalSpectrum:
                v = 255.0 * std::log1p(std::abs(f[c])) / logMax_;
                break;
            case DisplayMode::MaskView:
                v = 255.0 * static_cast<double>(h[c]);
                break;
            case DisplayMode::Phase:
                v = (std::atan2(f[c].imag(), f[c].real()) + kPi) / (2.0 * kPi) * 255.0;
                break;
        }
        const uint8_t b = toByte(v);
        out[0] = b; out[1] = b; out[2] = b; out[3] = 255;
    }
}

void FrequencySession::renderAll() {
    if (!hasF_) return;
    parallelForEach(P_, [&](int r) { renderPixelRow(r, 0, Q_); });
}

void FrequencySession::renderRect(const Rect& rect) {
    if (!hasF_ || rect.empty()) return;
    const int r0 = std::max(0, rect.r0), r1 = std::min(P_, rect.r1);
    const int c0 = std::max(0, rect.c0), c1 = std::min(Q_, rect.c1);
    for (int r = r0; r < r1; ++r) renderPixelRow(r, c0, c1);
}

Image FrequencySession::resultImage(OutputNormalization norm) {
    Image out(M_, N_, 255, bits_, dpi_);
    if (!hasResult_) return out;
    auto& d = out.getData();

    double mn = result_[0], mx = result_[0];
    for (double v : result_) { mn = std::min(mn, v); mx = std::max(mx, v); }

    for (int x = 0; x < M_; ++x) {
        for (int y = 0; y < N_; ++y) {
            const double g = result_[static_cast<size_t>(x) * N_ + y];
            double v;
            if (norm == OutputNormalization::MinMax && mx > mn) {
                v = (g - mn) * 255.0 / (mx - mn);
            } else {
                v = std::min(255.0, std::max(0.0, g));
            }
            d[static_cast<size_t>(x)][static_cast<size_t>(y)] = static_cast<int>(std::lround(v));
        }
    }
    return out;
}

}
