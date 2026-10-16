#include "ImageSpatialFilters.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace core {

namespace {

    bool kernelIsUsable(const Kernel& k) {
        return isValidKernelSize(k.size) &&
               k.weights.size() == static_cast<size_t>(k.size) * static_cast<size_t>(k.size);
    }

    int clampToByte(long long v) {
        return static_cast<int>(std::max(0LL, std::min(v, 255LL)));
    }

}

Kernel binomialKernel(int size) {
    Kernel k(size);
    if (!isValidKernelSize(size)) return k;

    // Linha do triangulo de Pascal: C(n, i) com n = size - 1
    const int n = size - 1;
    std::vector<double> row(static_cast<size_t>(size), 1.0);
    for (int i = 1; i < size; ++i) {
        row[static_cast<size_t>(i)] = row[static_cast<size_t>(i - 1)] * static_cast<double>(n - i + 1) / static_cast<double>(i);
    }

    double sum = 0.0;
    for (int i = 0; i < size; ++i) {
        for (int j = 0; j < size; ++j) {
            double w = row[static_cast<size_t>(i)] * row[static_cast<size_t>(j)];
            k.weights[static_cast<size_t>(i * size + j)] = w;
            sum += w;
        }
    }
    for (double& w : k.weights) w /= sum;
    return k;
}

Kernel gaussianKernel(int size, double sigma) {
    if (!isValidKernelSize(size)) return Kernel(size);
    if (!(sigma > 1e-9)) return identityKernel(size);

    Kernel k(size);
    double sum = 0.0;
    for (int s = -k.radius(); s <= k.radius(); ++s) {
        for (int t = -k.radius(); t <= k.radius(); ++t) {
            double h = std::exp(-static_cast<double>(s * s + t * t) / (2.0 * sigma * sigma));
            k.w(s, t) = h;
            sum += h;
        }
    }
    for (double& w : k.weights) w /= sum;
    return k;
}

Kernel laplacianKernel(LaplacianKind kind, bool negativeCenter) {
    Kernel k(3);
    const bool diagonals = (kind == LaplacianKind::Neighbors8);
    for (int s = -1; s <= 1; ++s) {
        for (int t = -1; t <= 1; ++t) {
            const bool isCenter = (s == 0 && t == 0);
            const bool isDiagonal = (s != 0 && t != 0);
            double w = 0.0;
            if (isCenter) w = diagonals ? -8.0 : -4.0;
            else if (!isDiagonal || diagonals) w = 1.0;
            k.w(s, t) = negativeCenter ? w : -w;
        }
    }
    return k;
}

Kernel sobelXKernel() {
    Kernel k(3);
    const double v[9] = {-1, -2, -1,
                          0,  0,  0,
                          1,  2,  1};
    for (int i = 0; i < 9; ++i) k.weights[static_cast<size_t>(i)] = v[i];
    return k;
}

Kernel sobelYKernel() {
    Kernel k(3);
    const double v[9] = {-1, 0, 1,
                         -2, 0, 2,
                         -1, 0, 1};
    for (int i = 0; i < 9; ++i) k.weights[static_cast<size_t>(i)] = v[i];
    return k;
}

Image medianFilter(Image& img, int size, BorderMode border) {
    Image out = img;

    const int M = img.getRows();
    const int N = img.getCols();
    if (M <= 0 || N <= 0 || !isValidKernelSize(size)) return out;

    const int a = size / 2;
    const int PM = M + 2 * a;
    const int PN = N + 2 * a;

    // Imagem preenchida com borda de espessura a (zeros ou replicada)
    std::vector<int> padded(static_cast<size_t>(PM) * static_cast<size_t>(PN), 0);
    auto& data = img.getData();
    for (int i = 0; i < PM; ++i) {
        for (int j = 0; j < PN; ++j) {
            int r = i - a;
            int c = j - a;
            if (r < 0 || r >= M || c < 0 || c >= N) {
                if (border == BorderMode::Zero) continue;
                r = std::max(0, std::min(r, M - 1));
                c = std::max(0, std::min(c, N - 1));
            }
            padded[static_cast<size_t>(i) * PN + j] = data[static_cast<size_t>(r)][static_cast<size_t>(c)];
        }
    }

    std::vector<int> window(static_cast<size_t>(size) * static_cast<size_t>(size));
    const size_t middle = window.size() / 2;   // posicao da mediana (impar: valor central)
    auto& outData = out.getData();

    for (int x = 0; x < M; ++x) {
        for (int y = 0; y < N; ++y) {
            size_t idx = 0;
            for (int s = 0; s < size; ++s) {
                const int* row = &padded[static_cast<size_t>(x + s) * PN + y];
                for (int t = 0; t < size; ++t) window[idx++] = row[t];
            }
            std::nth_element(window.begin(), window.begin() + static_cast<std::ptrdiff_t>(middle), window.end());
            outData[static_cast<size_t>(x)][static_cast<size_t>(y)] = window[middle];
        }
    }
    return out;
}

Image laplacian(Image& img, LaplacianKind kind, bool negativeCenter, BorderMode border) {
    // A mascara e simetrica: correlacao e convolucao dao o mesmo resultado
    return filterLinear(img, laplacianKernel(kind, negativeCenter), FilterOperation::Correlation, border);
}

Image sharpenLaplacian(Image& img, LaplacianKind kind, bool negativeCenter, BorderMode border) {
    Image lap = laplacian(img, kind, negativeCenter, border);
    const long long c = negativeCenter ? -1 : 1;

    Image out = img;
    for (int x = 0; x < img.getRows(); ++x) {
        for (int y = 0; y < img.getCols(); ++y) {
            long long g = static_cast<long long>(img.pixel(x, y)) + c * static_cast<long long>(lap.pixel(x, y));
            out.pixel(x, y) = clampToByte(g);
        }
    }
    return out;
}

Image highBoost(Image& img, const Kernel& blurKernel, double k, BorderMode border) {
    Image out = img;
    if (img.getRows() <= 0 || img.getCols() <= 0 || !kernelIsUsable(blurKernel)) return out;

    // Imagem borrada com 8 bits fracionarios: filtramos com a mascara
    // multiplicada por 256 e dividimos depois, para o arredondamento da
    // imagem borrada nao se somar ao erro da mascara (f - f_borrada).
    const double scale = 256.0;
    Kernel scaled = blurKernel;
    for (double& w : scaled.weights) w *= scale;
    Image blurred = filterLinear(img, scaled, FilterOperation::Correlation, border);

    for (int x = 0; x < img.getRows(); ++x) {
        for (int y = 0; y < img.getCols(); ++y) {
            const double f = static_cast<double>(img.pixel(x, y));
            const double fBlur = static_cast<double>(blurred.pixel(x, y)) / scale;
            const double g = f + k * (f - fBlur);   // g = f + k * mascara
            out.pixel(x, y) = clampToByte(static_cast<long long>(std::llround(g)));
        }
    }
    return out;
}

Image sobelX(Image& img, BorderMode border) {
    return filterLinear(img, sobelXKernel(), FilterOperation::Correlation, border);
}

Image sobelY(Image& img, BorderMode border) {
    return filterLinear(img, sobelYKernel(), FilterOperation::Correlation, border);
}

Image gradientMagnitude(Image& img, BorderMode border, bool approximate) {
    Image gx = sobelX(img, border);
    Image gy = sobelY(img, border);

    Image out = img;
    for (int x = 0; x < img.getRows(); ++x) {
        for (int y = 0; y < img.getCols(); ++y) {
            const long long a = gx.pixel(x, y);
            const long long b = gy.pixel(x, y);
            if (approximate) {
                out.pixel(x, y) = static_cast<int>(std::llabs(a) + std::llabs(b));
            } else {
                out.pixel(x, y) = static_cast<int>(std::llround(std::sqrt(static_cast<double>(a * a + b * b))));
            }
        }
    }
    return out;
}

}
