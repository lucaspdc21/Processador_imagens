#include "ImageConvolution.hpp"
#include <algorithm>
#include <climits>
#include <cmath>

namespace core {

namespace {

    int roundToInt(double v) {
        if (!std::isfinite(v)) return 0;
        const double limit = 1.0e9;   // evita estouro de int
        v = std::max(-limit, std::min(v, limit));
        return static_cast<int>(std::llround(v));
    }

    bool kernelIsConsistent(const Kernel& k) {
        return isValidKernelSize(k.size) &&
               k.weights.size() == static_cast<size_t>(k.size) * static_cast<size_t>(k.size);
    }

}

bool isValidKernelSize(int size) {
    return size >= 1 && size % 2 == 1;
}

Kernel identityKernel(int size) {
    Kernel k(size);
    if (isValidKernelSize(size)) k.w(0, 0) = 1.0;
    return k;
}

Kernel boxKernel(int size) {
    Kernel k(size);
    if (!isValidKernelSize(size)) return k;
    const double value = 1.0 / (static_cast<double>(size) * static_cast<double>(size));
    std::fill(k.weights.begin(), k.weights.end(), value);
    return k;
}

double kernelSum(const Kernel& kernel) {
    double sum = 0.0;
    for (double w : kernel.weights) sum += w;
    return sum;
}

Kernel rotateKernel180(const Kernel& kernel) {
    // Girar 180 graus equivale a w'(s, t) = w(-s, -t), isto e, inverter a
    // ordem dos coeficientes guardados linha a linha.
    Kernel rotated = kernel;
    std::reverse(rotated.weights.begin(), rotated.weights.end());
    return rotated;
}

Image filterLinear(Image& img, const Kernel& kernel, FilterOperation operation, BorderMode border) {
    Image out = img;

    const int M = img.getRows();
    const int N = img.getCols();
    if (M <= 0 || N <= 0 || !kernelIsConsistent(kernel)) return out;

    // Convolucao = correlacao com a mascara girada 180 graus (Eq. 3.4-2 e 3.4-1)
    const Kernel k = (operation == FilterOperation::Convolution) ? rotateKernel180(kernel) : kernel;
    const int m = k.size;
    const int a = k.radius();

    // Imagem preenchida com uma borda de espessura a em cada lado. Assim cada
    // pixel da mascara percorre todos os pixels da imagem, e o resultado ja
    // sai com o tamanho M x N (equivale a "preencher e recortar" do texto).
    const int PM = M + 2 * a;
    const int PN = N + 2 * a;
    std::vector<int> padded(static_cast<size_t>(PM) * static_cast<size_t>(PN), 0);

    auto& data = img.getData();
    for (int i = 0; i < PM; ++i) {
        for (int j = 0; j < PN; ++j) {
            int r = i - a;
            int c = j - a;
            if (r < 0 || r >= M || c < 0 || c >= N) {
                if (border == BorderMode::Zero) continue;   // ja e 0
                r = std::max(0, std::min(r, M - 1));
                c = std::max(0, std::min(c, N - 1));
            }
            padded[static_cast<size_t>(i) * PN + j] = data[static_cast<size_t>(r)][static_cast<size_t>(c)];
        }
    }

    // g(x, y) = soma_s soma_t w(s, t) f(x + s, y + t)    (Eq. 3.4-1)
    auto& outData = out.getData();
    for (int x = 0; x < M; ++x) {
        for (int y = 0; y < N; ++y) {
            double sum = 0.0;
            for (int s = 0; s < m; ++s) {
                const int* pixels = &padded[static_cast<size_t>(x + s) * PN + y];
                const double* coeffs = &k.weights[static_cast<size_t>(s) * m];
                for (int t = 0; t < m; ++t) {
                    sum += coeffs[t] * pixels[t];
                }
            }
            outData[static_cast<size_t>(x)][static_cast<size_t>(y)] = roundToInt(sum);
        }
    }
    return out;
}

void applyFilter(Image& img, const Kernel& kernel, FilterOperation operation,
                 BorderMode border, bool clampToByte) {
    Image result = filterLinear(img, kernel, operation, border);
    if (clampToByte) result.clampToRange();
    img = result;
}

Image normalizeForDisplay(Image& img) {
    Image out = img;
    auto& data = out.getData();
    if (img.getRows() <= 0 || img.getCols() <= 0) return out;

    int mn = INT_MAX;
    int mx = INT_MIN;
    for (const auto& row : data) {
        for (int v : row) {
            mn = std::min(mn, v);
            mx = std::max(mx, v);
        }
    }

    for (auto& row : data) {
        for (int& v : row) {
            if (mx > mn) {
                v = static_cast<int>(std::llround((static_cast<double>(v) - mn) * 255.0 / (static_cast<double>(mx) - mn)));
            } else {
                v = std::max(0, std::min(v, 255));
            }
        }
    }
    return out;
}

}
