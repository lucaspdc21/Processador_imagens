#include "ImageRotate.hpp"
#include <algorithm>
#include <cmath>

namespace core {

namespace {
    constexpr double kPi = 3.14159265358979323846;

    // Angulo assinado em radianos, ja com o sinal da constante unica.
    double signedRadians(double angleDegrees) {
        return kCounterClockwiseSign * angleDegrees * kPi / 180.0;
    }
}

void defaultPivot(int rows, int cols, double& px, double& py) {
    px = (static_cast<double>(cols) - 1.0) / 2.0;
    py = (static_cast<double>(rows) - 1.0) / 2.0;
}

Affine2D rotationAboutPivot(double angleDegrees, double px, double py) {
    // p' = T(c) * R * T(-c) * p
    return Affine2D::translation(px, py)
         * Affine2D::rotationImageCoords(signedRadians(angleDegrees))
         * Affine2D::translation(-px, -py);
}

bool computeRotationLayout(int rows, int cols, double angleDegrees, double px, double py,
                           RotationCanvas canvas, RotationLayout& layout) {
    if (rows <= 0 || cols <= 0) return false;
    if (!std::isfinite(angleDegrees) || !std::isfinite(px) || !std::isfinite(py)) return false;

    if (canvas == RotationCanvas::KeepSize) {
        layout.outRows = rows;
        layout.outCols = cols;
        layout.offsetX = 0.0;
        layout.offsetY = 0.0;
        return static_cast<double>(rows) * cols <= static_cast<double>(kMaxOutputPixels);
    }

    // Expandir: retangulo envolvente dos 4 cantos do retangulo continuo.
    const Affine2D fwd = rotationAboutPivot(angleDegrees, px, py);
    const double w = static_cast<double>(cols);
    const double h = static_cast<double>(rows);
    const double cx[4] = {-0.5, w - 0.5, w - 0.5, -0.5};
    const double cy[4] = {-0.5, -0.5, h - 0.5, h - 0.5};

    double xmin = 1e300, xmax = -1e300, ymin = 1e300, ymax = -1e300;
    for (int i = 0; i < 4; ++i) {
        double x, y;
        fwd.apply(cx[i], cy[i], x, y);
        xmin = std::min(xmin, x);  xmax = std::max(xmax, x);
        ymin = std::min(ymin, y);  ymax = std::max(ymax, y);
    }

    const double eps = 1e-6;
    const double wo = std::max(1.0, std::ceil(xmax - xmin - eps));
    const double ho = std::max(1.0, std::ceil(ymax - ymin - eps));
    if (wo * ho > static_cast<double>(kMaxOutputPixels)) return false;

    layout.outCols = static_cast<int>(wo);
    layout.outRows = static_cast<int>(ho);
    layout.offsetX = xmin + 0.5;
    layout.offsetY = ymin + 0.5;
    return true;
}

Image rotateImage(Image& src, double angleDegrees, double px, double py,
                  Interpolation interp, RotationCanvas canvas, int background) {
    RotationLayout layout;
    if (!computeRotationLayout(src.getRows(), src.getCols(), angleDegrees, px, py, canvas, layout)) {
        return Image(0, 0);
    }

    // Inversa construida diretamente (R^T e a inversa de R, sem inverter a
    // matriz numericamente):  p = T(c) * R^T * T(o - c) * p'
    const Affine2D rT = Affine2D::rotationImageCoords(-signedRadians(angleDegrees));
    const Affine2D aInv = Affine2D::translation(px, py)
                        * rT
                        * Affine2D::translation(layout.offsetX - px, layout.offsetY - py);

    return warpInverse(src, aInv, layout.outRows, layout.outCols, interp, background);
}

}
