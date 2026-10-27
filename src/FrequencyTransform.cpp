#include "FrequencyTransform.hpp"
#include <algorithm>
#include <cmath>
#include <functional>
#include <thread>

namespace core {

namespace {

    constexpr double kTwoPi = 6.283185307179586476925286766559;

    // Executa fn(i) para i = 0..n-1 em varios threads (agendamento dinamico).
    template <class Fn>
    void parallelFor(int n, Fn&& fn) {
        const unsigned hw = std::max(1u, std::thread::hardware_concurrency());
        const int nt = static_cast<int>(std::min<unsigned>(hw, static_cast<unsigned>(std::max(n, 1))));
        if (nt <= 1) {
            for (int i = 0; i < n; ++i) fn(i);
            return;
        }
        std::atomic<int> next{0};
        auto worker = [&]() {
            for (int i = next.fetch_add(1); i < n; i = next.fetch_add(1)) fn(i);
        };
        std::vector<std::thread> pool;
        pool.reserve(static_cast<size_t>(nt - 1));
        for (int t = 1; t < nt; ++t) pool.emplace_back(worker);
        worker();
        for (auto& th : pool) th.join();
    }

    // ---------------------------------------------------------------------
    // FFT radix-2 iterativa (Secao 4.11.3, "duplicacao sucessiva").
    // Para M = 2^n, K = M/2 e W_M = e^{-j2pi/M}:
    //   F(u)     = F_par(u) + F_impar(u) * W_M^u
    //   F(u + K) = F_par(u) - F_impar(u) * W_M^u        (u = 0..K-1)
    // A ordem de inversao de bits coloca pares e impares lado a lado; cada
    // estagio "duplica" o tamanho das transformadas parciais combinando-as
    // pelas duas formulas acima (borboletas). Custo: O(M log2 M).
    // ---------------------------------------------------------------------
    struct FftPlan {
        int n = 0;
        std::vector<int> rev;          // permutacao de inversao de bits
        std::vector<Complex> tw;       // W_n^k, k = 0..n/2-1

        explicit FftPlan(int size) : n(size) {
            rev.assign(static_cast<size_t>(n), 0);
            int bits = 0;
            while ((1 << bits) < n) ++bits;
            for (int i = 0; i < n; ++i) {
                int r = 0;
                for (int b = 0; b < bits; ++b)
                    if (i & (1 << b)) r |= 1 << (bits - 1 - b);
                rev[static_cast<size_t>(i)] = r;
            }
            tw.resize(static_cast<size_t>(std::max(1, n / 2)));
            for (int k = 0; k < n / 2; ++k) {
                const double ang = -kTwoPi * static_cast<double>(k) / static_cast<double>(n);
                tw[static_cast<size_t>(k)] = Complex(std::cos(ang), std::sin(ang));
            }
        }
    };

    void fftRadix2InPlace(Complex* a, const FftPlan& plan) {
        const int n = plan.n;
        for (int i = 0; i < n; ++i) {
            const int j = plan.rev[static_cast<size_t>(i)];
            if (i < j) std::swap(a[i], a[j]);
        }
        for (int len = 2; len <= n; len <<= 1) {
            const int half = len >> 1;
            const int step = n / len;             // W_len^j = W_n^{j*step}
            for (int i = 0; i < n; i += len) {
                for (int j = 0; j < half; ++j) {
                    const Complex w = plan.tw[static_cast<size_t>(j * step)];
                    const double xr = a[i + j + half].real(), xi = a[i + j + half].imag();
                    const double vr = xr * w.real() - xi * w.imag();    // F_impar * W^u
                    const double vi = xr * w.imag() + xi * w.real();
                    const double ur = a[i + j].real(), ui = a[i + j].imag();   // F_par
                    a[i + j]        = Complex(ur + vr, ui + vi);        // F(u)
                    a[i + j + half] = Complex(ur - vr, ui - vi);        // F(u + K)
                }
            }
        }
    }

    // Transpoe 'src' (r x c) em 'dst' (c x r), em blocos (melhor uso do cache).
    void transposeBlocked(const ComplexMatrix& src, ComplexMatrix& dst) {
        dst.rows = src.cols;
        dst.cols = src.rows;
        dst.data.resize(src.data.size());
        const int B = 32;
        const int blocksR = (src.rows + B - 1) / B;
        parallelFor(blocksR, [&](int bi) {
            const int r0 = bi * B;
            const int r1 = std::min(src.rows, r0 + B);
            for (int c0 = 0; c0 < src.cols; c0 += B) {
                const int c1 = std::min(src.cols, c0 + B);
                for (int r = r0; r < r1; ++r)
                    for (int c = c0; c < c1; ++c)
                        dst.data[static_cast<size_t>(c) * dst.cols + r] = src.data[static_cast<size_t>(r) * src.cols + c];
            }
        });
    }

    // Aplica uma transformada 1-D a todas as linhas de 'm', no proprio lugar.
    // 'rowFn(ptr, n)' transforma a linha ptr[0..n-1]. Atualiza o progresso
    // entre progBase e progBase + progSpan. Retorna false se cancelado.
    bool transformRows(ComplexMatrix& m, const std::function<void(Complex*, int)>& rowFn,
                       TransformControl* control, float progBase, float progSpan) {
        std::atomic<int> done{0};
        std::atomic<bool> cancelled{false};
        const int total = m.rows;
        parallelFor(total, [&](int r) {
            if (cancelled.load(std::memory_order_relaxed)) return;
            if (control && control->cancel.load(std::memory_order_relaxed)) {
                cancelled.store(true, std::memory_order_relaxed);
                return;
            }
            rowFn(&m.data[static_cast<size_t>(r) * m.cols], m.cols);
            const int d = done.fetch_add(1, std::memory_order_relaxed) + 1;
            if (control && (d % 8 == 0 || d == total))
                control->progress.store(progBase + progSpan * static_cast<float>(d) / static_cast<float>(total),
                                        std::memory_order_relaxed);
        });
        return !cancelled.load();
    }

    // Separabilidade (Secao 4.11.1): FFT 1-D em todas as linhas e depois em
    // todas as colunas do resultado. As colunas sao tratadas transpondo a
    // matriz (acesso sequencial) e transpondo de volta.
    bool forwardRadix(ComplexMatrix& m, TransformControl* control) {
        if (!isPowerOfTwo(m.rows) || !isPowerOfTwo(m.cols)) return false;
        const FftPlan planC(m.cols);
        const FftPlan planR(m.rows);
        const auto rowFn = [](const FftPlan& plan) {
            return [&plan](Complex* row, int) { fftRadix2InPlace(row, plan); };
        };
        // 1) linhas (comprimento = cols)
        if (!transformRows(m, rowFn(planC), control, 0.00f, 0.45f)) return false;
        // 2) colunas: transpoe, transforma as linhas (comprimento = rows), transpoe de volta
        ComplexMatrix t;
        transposeBlocked(m, t);
        if (!transformRows(t, rowFn(planR), control, 0.45f, 0.45f)) return false;
        transposeBlocked(t, m);
        if (control) control->progress.store(1.0f, std::memory_order_relaxed);
        return true;
    }

}

// ---------------------------------------------------------------------------
// API publica
// ---------------------------------------------------------------------------

bool isPowerOfTwo(int n) {
    return n > 0 && (n & (n - 1)) == 0;
}

int nextPowerOfTwo(int n) {
    int p = 1;
    while (p < n && p < (1 << 30)) p <<= 1;
    return p;
}

void parallelForEach(int n, const std::function<void(int)>& fn) {
    parallelFor(n, fn);
}

std::vector<Complex> fft1d(const std::vector<Complex>& f) {
    const int n = static_cast<int>(f.size());
    if (!isPowerOfTwo(n)) return {};
    std::vector<Complex> out = f;
    const FftPlan plan(n);
    fftRadix2InPlace(out.data(), plan);
    return out;
}

bool forwardTransform2D(ComplexMatrix& m, TransformControl* control) {
    if (m.rows <= 0 || m.cols <= 0 ||
        m.data.size() != static_cast<size_t>(m.rows) * static_cast<size_t>(m.cols)) return false;
    if (control) control->progress.store(0.0f, std::memory_order_relaxed);
    return forwardRadix(m, control);
}

bool inverseTransform2D(ComplexMatrix& m, TransformControl* control) {
    // Secao 4.11.2:  f = conj( FFT{ conj(F) } ) / (M N)
    for (auto& v : m.data) v = std::conj(v);
    if (!forwardTransform2D(m, control)) return false;
    const double inv = 1.0 / (static_cast<double>(m.rows) * static_cast<double>(m.cols));
    parallelFor(m.rows, [&](int r) {
        Complex* row = &m.data[static_cast<size_t>(r) * m.cols];
        for (int c = 0; c < m.cols; ++c) row[c] = std::conj(row[c]) * inv;
    });
    return true;
}

}
