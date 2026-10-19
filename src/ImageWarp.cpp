#include "ImageWarp.hpp"
#include <algorithm>
#include <cmath>

namespace core {

// ---------------------------------------------------------------------------
// Matrizes afins (Eq. 2.6-23, Tabela 2.2)
// ---------------------------------------------------------------------------

Affine2D Affine2D::identity() {
    return Affine2D();
}

Affine2D Affine2D::scale(double sx, double sy) {
    Affine2D a;
    a.at(0, 0) = sx;
    a.at(1, 1) = sy;
    return a;
}

Affine2D Affine2D::translation(double tx, double ty) {
    Affine2D a;
    a.at(0, 2) = tx;
    a.at(1, 2) = ty;
    return a;
}

Affine2D Affine2D::rotationImageCoords(double radians) {
    // Com y para baixo, a rotacao anti-horaria visual de angulo theta leva
    // um ponto da entrada para a saida por R = [[cos, sin], [-sin, cos]].
    double c = std::cos(radians);
    double s = std::sin(radians);
    // Em multiplos de 90 graus, cos/sen saem como 6e-17 em vez de 0; ajustar
    // para valores exatos deixa a rotacao de 90/180/270 graus exata.
    const double snap = 1e-12;
    if (std::fabs(c) < snap) c = 0.0;
    if (std::fabs(s) < snap) s = 0.0;
    if (std::fabs(c - 1.0) < snap) c = 1.0;
    if (std::fabs(c + 1.0) < snap) c = -1.0;
    if (std::fabs(s - 1.0) < snap) s = 1.0;
    if (std::fabs(s + 1.0) < snap) s = -1.0;
    Affine2D a;
    a.at(0, 0) = c;  a.at(0, 1) = s;
    a.at(1, 0) = -s; a.at(1, 1) = c;
    return a;
}

Affine2D Affine2D::operator*(const Affine2D& other) const {
    Affine2D out;
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 3; ++c) {
            double sum = 0.0;
            for (int k = 0; k < 3; ++k) sum += at(r, k) * other.at(k, c);
            out.at(r, c) = sum;
        }
    }
    return out;
}

bool Affine2D::inverse(Affine2D& out) const {
    const double a = at(0, 0), b = at(0, 1), tx = at(0, 2);
    const double c = at(1, 0), d = at(1, 1), ty = at(1, 2);
    const double det = a * d - b * c;
    if (std::fabs(det) < 1e-300) return false;

    const double ia = d / det, ib = -b / det;
    const double ic = -c / det, id = a / det;
    out = Affine2D();
    out.at(0, 0) = ia; out.at(0, 1) = ib; out.at(0, 2) = -(ia * tx + ib * ty);
    out.at(1, 0) = ic; out.at(1, 1) = id; out.at(1, 2) = -(ic * tx + id * ty);
    return true;
}

// ---------------------------------------------------------------------------
// Interpolacao
// ---------------------------------------------------------------------------

namespace {

    inline int clampIndex(int v, int hi) {
        return v < 0 ? 0 : (v > hi ? hi : v);
    }

    // Versoes internas que trabalham sobre ponteiros de linha, sem o custo de
    // chamar Image::getData() a cada pixel.
    inline double nearestAt(const std::vector<std::vector<int>>& d, int rows, int cols,
                            double xs, double ys) {
        const int xi = clampIndex(static_cast<int>(std::floor(xs + 0.5)), cols - 1);
        const int yi = clampIndex(static_cast<int>(std::floor(ys + 0.5)), rows - 1);
        return static_cast<double>(d[static_cast<size_t>(yi)][static_cast<size_t>(xi)]);
    }

    inline double bilinearAt(const std::vector<std::vector<int>>& d, int rows, int cols,
                             double xs, double ys) {
        const double fx0 = std::floor(xs);
        const double fy0 = std::floor(ys);
        const double dx = xs - fx0;
        const double dy = ys - fy0;

        // Limita antes de converter para int (evita overflow com coordenadas
        // absurdas); depois aplica o clamp dos vizinhos a [0, W-1] e [0, H-1].
        const int ix = static_cast<int>(std::max(-2.0, std::min(fx0, static_cast<double>(cols) + 1.0)));
        const int iy = static_cast<int>(std::max(-2.0, std::min(fy0, static_cast<double>(rows) + 1.0)));
        const int x0 = clampIndex(ix, cols - 1);
        const int x1 = clampIndex(ix + 1, cols - 1);
        const int y0 = clampIndex(iy, rows - 1);
        const int y1 = clampIndex(iy + 1, rows - 1);

        const std::vector<int>& r0 = d[static_cast<size_t>(y0)];
        const std::vector<int>& r1 = d[static_cast<size_t>(y1)];

        return (1.0 - dx) * (1.0 - dy) * r0[static_cast<size_t>(x0)]
             +        dx  * (1.0 - dy) * r0[static_cast<size_t>(x1)]
             + (1.0 - dx) *        dy  * r1[static_cast<size_t>(x0)]
             +        dx  *        dy  * r1[static_cast<size_t>(x1)];
    }

}

double sampleNearest(Image& img, double xs, double ys) {
    if (img.getRows() <= 0 || img.getCols() <= 0) return 0.0;
    return nearestAt(img.getData(), img.getRows(), img.getCols(), xs, ys);
}

double sampleBilinear(Image& img, double xs, double ys) {
    if (img.getRows() <= 0 || img.getCols() <= 0) return 0.0;
    return bilinearAt(img.getData(), img.getRows(), img.getCols(), xs, ys);
}

// ---------------------------------------------------------------------------
// Warp por mapeamento inverso (Secao 2.6.5)
// ---------------------------------------------------------------------------

Image warpInverse(Image& src, const Affine2D& aInv, int outRows, int outCols,
                  Interpolation interp, int background) {
    if (outRows <= 0 || outCols <= 0) return Image(0, 0);

    const int rows = src.getRows();
    const int cols = src.getCols();
    Image out(outRows, outCols, src.getMaxIntensity(), src.getBitsPerPixel(), src.getDpi());
    if (rows <= 0 || cols <= 0) {
        out.fill(background);
        return out;
    }

    const auto& in = src.getData();
    auto& outData = out.getData();

    // Dominio valido para amostragem: [-0.5, W-0.5] x [-0.5, H-0.5]
    // (com uma pequena tolerancia numerica nas bordas).
    const double eps = 1e-6;
    const double xMin = -0.5 - eps, xMax = static_cast<double>(cols) - 0.5 + eps;
    const double yMin = -0.5 - eps, yMax = static_cast<double>(rows) - 0.5 + eps;

    const double a00 = aInv.at(0, 0), a01 = aInv.at(0, 1), a02 = aInv.at(0, 2);
    const double a10 = aInv.at(1, 0), a11 = aInv.at(1, 1), a12 = aInv.at(1, 2);

    for (int yo = 0; yo < outRows; ++yo) {
        std::vector<int>& outRow = outData[static_cast<size_t>(yo)];
        // Parte da posicao que nao depende de xo.
        const double baseX = a01 * yo + a02;
        const double baseY = a11 * yo + a12;
        for (int xo = 0; xo < outCols; ++xo) {
            const double xs = a00 * xo + baseX;
            const double ys = a10 * xo + baseY;

            if (xs < xMin || xs > xMax || ys < yMin || ys > yMax) {
                outRow[static_cast<size_t>(xo)] = background;
                continue;
            }

            const double v = (interp == Interpolation::Nearest)
                                 ? nearestAt(in, rows, cols, xs, ys)
                                 : bilinearAt(in, rows, cols, xs, ys);
            outRow[static_cast<size_t>(xo)] = static_cast<int>(std::lround(v));
        }
    }
    return out;
}

}
