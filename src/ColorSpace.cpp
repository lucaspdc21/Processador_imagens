#include "ColorSpace.hpp"
#include <algorithm>
#include <cmath>

namespace core {

namespace {

    const double kPi = 3.14159265358979323846;
    const double kDegToRad = kPi / 180.0;
    const double kRadToDeg = 180.0 / kPi;
    const double kSatEps = 1e-6;   // abaixo disso a cor e considerada cinza (S = 0)

    inline double clamp01(double v) { return v < 0.0 ? 0.0 : (v > 1.0 ? 1.0 : v); }

    inline int toByte(double v) {
        return static_cast<int>(std::lround(clamp01(v) * 255.0));
    }

}

Hsi rgbToHsi(double r, double g, double b) {
    Hsi out;

    // I = (R + G + B) / 3                                        (Eq. 6.2-4)
    out.i = (r + g + b) / 3.0;

    // S = 1 - 3 / (R + G + B) * min(R, G, B) = 1 - min / I       (Eq. 6.2-3)
    // Preto (R + G + B = 0): S = 0
    const double mn = std::min(r, std::min(g, b));
    out.s = (out.i <= 1e-12) ? 0.0 : 1.0 - mn / out.i;
    if (out.s < 0.0) out.s = 0.0;

    // Matiz indefinido quando S = 0 (preto, branco e cinzas): H = 0
    if (out.s < kSatEps) {
        out.s = 0.0;
        out.h = 0.0;
        return out;
    }

    // Eq. 6.2-2:  theta = arccos{ 0.5[(R-G)+(R-B)] / sqrt[(R-G)^2 + (R-B)(G-B)] }
    //             H = theta se B <= G, 360 - theta se B > G.
    // Calculado com atan2, que da exatamente o mesmo H sem dividir por zero
    // nem perder precisao perto de 0 e 180 graus:
    //   cos(theta) = [R - (G+B)/2] / den,  sen(theta) = (sqrt(3)/2)(G-B) / den,
    // com den = sqrt[(R-G)^2 + (R-B)(G-B)]. atan2 devolve theta em [0, 180]
    // quando G >= B e o angulo negativo -theta quando G < B (somar 360 da
    // 360 - theta).
    const double y = std::sqrt(3.0) / 2.0 * (g - b);
    const double x = r - 0.5 * (g + b);
    double h = std::atan2(y, x) * kRadToDeg;
    if (h < 0.0) h += 360.0;
    if (h >= 360.0) h -= 360.0;
    out.h = h;
    return out;
}

void hsiToRgb(const Hsi& hsi, double& r, double& g, double& b) {
    double h = std::fmod(hsi.h, 360.0);
    if (h < 0.0) h += 360.0;

    const double s = clamp01(hsi.s);
    const double i = hsi.i < 0.0 ? 0.0 : hsi.i;

    // Setor: RG (0..120), GB (120..240), BR (240..360). Em cada setor o
    // angulo e medido a partir do inicio do setor (Hs em [0, 120)).
    const int sector = std::min(2, static_cast<int>(h / 120.0));
    const double hs = h - 120.0 * sector;

    // Os tres setores tem a mesma estrutura:
    //   x = I (1 - S)
    //   y = I [1 + S cos(Hs) / cos(60 - Hs)]
    //   z = 3I - (x + y)
    // cos(60 - Hs) nunca e zero, pois Hs esta em [0, 120).
    const double x = i * (1.0 - s);
    const double y = i * (1.0 + s * std::cos(hs * kDegToRad) / std::cos((60.0 - hs) * kDegToRad));
    const double z = 3.0 * i - (x + y);

    switch (sector) {
        case 0:  r = y; g = z; b = x; break;   // RG:  B = x, R = y, G = z
        case 1:  r = x; g = y; b = z; break;   // GB:  R = x, G = y, B = z
        default: r = z; g = x; b = y; break;   // BR:  G = x, B = y, R = z
    }
    r = clamp01(r);
    g = clamp01(g);
    b = clamp01(b);
}

HsiImage rgbToHsi(ColorImage& img) {
    HsiImage out;
    out.rows = img.getRows();
    out.cols = img.getCols();
    const size_t n = static_cast<size_t>(out.rows) * static_cast<size_t>(out.cols);
    out.h.assign(n, 0.0f);
    out.s.assign(n, 0.0f);
    out.i.assign(n, 0.0f);

    auto& dr = img.r().getData();
    auto& dg = img.g().getData();
    auto& db = img.b().getData();
    for (int y = 0; y < out.rows; ++y) {
        for (int x = 0; x < out.cols; ++x) {
            Hsi p = rgbToHsi(dr[y][x] / 255.0, dg[y][x] / 255.0, db[y][x] / 255.0);
            const size_t k = out.index(y, x);
            out.h[k] = static_cast<float>(p.h);
            out.s[k] = static_cast<float>(p.s);
            out.i[k] = static_cast<float>(p.i);
        }
    }
    return out;
}

ColorImage hsiToRgb(const HsiImage& hsi, ColorImage& like) {
    ColorImage out(hsi.rows, hsi.cols, 255, like.getBitsPerPixel(), like.getDpi());
    auto& dr = out.r().getData();
    auto& dg = out.g().getData();
    auto& db = out.b().getData();
    for (int y = 0; y < hsi.rows; ++y) {
        for (int x = 0; x < hsi.cols; ++x) {
            const size_t k = hsi.index(y, x);
            Hsi p;
            p.h = hsi.h[k];
            p.s = hsi.s[k];
            p.i = hsi.i[k];
            double r, g, b;
            hsiToRgb(p, r, g, b);
            dr[y][x] = toByte(r);
            dg[y][x] = toByte(g);
            db[y][x] = toByte(b);
        }
    }
    return out;
}

Image hsiPlaneToImage(const HsiImage& hsi, HsiPlane plane) {
    Image out(hsi.rows, hsi.cols);
    auto& d = out.getData();
    for (int y = 0; y < hsi.rows; ++y) {
        for (int x = 0; x < hsi.cols; ++x) {
            const size_t k = hsi.index(y, x);
            double v = 0.0;
            switch (plane) {
                case HsiPlane::H: v = hsi.h[k] / 360.0; break;
                case HsiPlane::S: v = hsi.s[k]; break;
                default:          v = hsi.i[k]; break;
            }
            d[y][x] = toByte(v);
        }
    }
    return out;
}

}
