#include "ImageColor.hpp"
#include "ImageResample.hpp"
#include "ImageSpatialFilters.hpp"
#include <algorithm>
#include <cmath>

namespace core {

namespace {

    inline int clampByte(double v) {
        if (v <= 0.0) return 0;
        if (v >= 255.0) return 255;
        return static_cast<int>(std::lround(v));
    }

    inline double clamp01(double v) { return v < 0.0 ? 0.0 : (v > 1.0 ? 1.0 : v); }

    inline int pix(Image& img, int y, int x) {
        return std::max(0, std::min(img.getData()[static_cast<size_t>(y)][static_cast<size_t>(x)], 255));
    }

    // Substitui o plano I do HSI pelo plano 'intensity' (0..255) e volta a RGB
    void replaceIntensity(ColorImage& img, Image& intensity) {
        HsiImage hsi = rgbToHsi(img);
        if (intensity.getRows() != hsi.rows || intensity.getCols() != hsi.cols) return;
        intensity.clampToRange();
        auto& d = intensity.getData();
        for (int y = 0; y < hsi.rows; ++y) {
            for (int x = 0; x < hsi.cols; ++x) {
                hsi.i[hsi.index(y, x)] = static_cast<float>(d[static_cast<size_t>(y)][static_cast<size_t>(x)] / 255.0);
            }
        }
        img = hsiToRgb(hsi, img);
    }

    // Plano cinza ponderado, em 8 bits
    Image grayPlane(ColorImage& img, double wR, double wG, double wB) {
        double sum = wR + wG + wB;
        if (!(sum > 1e-12) || wR < 0.0 || wG < 0.0 || wB < 0.0) {
            wR = 0.299; wG = 0.587; wB = 0.114; sum = 1.0;
        }
        wR /= sum; wG /= sum; wB /= sum;

        Image out(img.getRows(), img.getCols(), 255, img.getBitsPerPixel(), img.getDpi());
        for (int y = 0; y < img.getRows(); ++y) {
            for (int x = 0; x < img.getCols(); ++x) {
                out.pixel(y, x) = clampByte(wR * pix(img.r(), y, x) + wG * pix(img.g(), y, x) + wB * pix(img.b(), y, x));
            }
        }
        return out;
    }

}

void applyToPlanes(ColorImage& img, ColorTarget target, const PlaneOp& op) {
    if (img.getRows() <= 0 || img.getCols() <= 0) return;

    if (target == ColorTarget::ChannelsRGB) {
        for (int c = 0; c < ColorImage::kChannels; ++c) op(img.channel(c));
        return;
    }

    HsiImage hsi = rgbToHsi(img);
    Image intensity = hsiPlaneToImage(hsi, HsiPlane::I);
    op(intensity);
    replaceIntensity(img, intensity);
}

void mapPlanes(ColorImage& img, ColorTarget target, const PlaneFn& fn) {
    if (img.getRows() <= 0 || img.getCols() <= 0) return;

    if (target == ColorTarget::ChannelsRGB) {
        for (int c = 0; c < ColorImage::kChannels; ++c) img.channel(c) = fn(img.channel(c));
        return;
    }

    HsiImage hsi = rgbToHsi(img);
    Image intensity = hsiPlaneToImage(hsi, HsiPlane::I);
    Image result = fn(intensity);
    replaceIntensity(img, result);
}

void toGrayAverage(ColorImage& img) {
    Image y = intensityPlane(img);
    for (int c = 0; c < ColorImage::kChannels; ++c) img.channel(c) = y;
}

void toGrayWeighted(ColorImage& img, double wR, double wG, double wB) {
    Image y = grayPlane(img, wR, wG, wB);
    for (int c = 0; c < ColorImage::kChannels; ++c) img.channel(c) = y;
}

void sepia(ColorImage& img, const SepiaParams& params) {
    const int rows = img.getRows();
    const int cols = img.getCols();
    if (rows <= 0 || cols <= 0) return;

    if (params.mode == SepiaMode::Matrix) {
        ColorImage src = img;
        for (int y = 0; y < rows; ++y) {
            for (int x = 0; x < cols; ++x) {
                const double r = pix(src.r(), y, x);
                const double g = pix(src.g(), y, x);
                const double b = pix(src.b(), y, x);
                img.r().pixel(y, x) = clampByte(0.393 * r + 0.769 * g + 0.189 * b);
                img.g().pixel(y, x) = clampByte(0.349 * r + 0.686 * g + 0.168 * b);
                img.b().pixel(y, x) = clampByte(0.272 * r + 0.534 * g + 0.131 * b);
            }
        }
        return;
    }

    // Via HSI: I = cinza ponderado, H e S fixos
    Image yPlane = grayPlane(img, 0.299, 0.587, 0.114);
    HsiImage hsi;
    hsi.rows = rows;
    hsi.cols = cols;
    const size_t n = static_cast<size_t>(rows) * static_cast<size_t>(cols);
    hsi.h.assign(n, static_cast<float>(params.hue));
    hsi.s.assign(n, static_cast<float>(clamp01(params.saturation)));
    hsi.i.assign(n, 0.0f);
    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < cols; ++x) {
            hsi.i[hsi.index(y, x)] = static_cast<float>(yPlane.pixel(y, x) / 255.0);
        }
    }
    img = hsiToRgb(hsi, img);
}

double chromaKeyMaxDistance(ChromaKeyMode mode) {
    return mode == ChromaKeyMode::Sphere ? 255.0 * std::sqrt(3.0) : 255.0;
}

void chromaKey(ColorImage& front, ColorImage& background, const ChromaKeyParams& params) {
    const int rows = front.getRows();
    const int cols = front.getCols();
    if (rows <= 0 || cols <= 0 || background.getRows() <= 0 || background.getCols() <= 0) return;

    // Fundo no tamanho da frente
    ColorImage bg = background;
    if (bg.getRows() != rows || bg.getCols() != cols) {
        for (int c = 0; c < ColorImage::kChannels; ++c) {
            resample(bg.channel(c), rows, cols, resampleType::Bilinear);
        }
    }

    const double dMax = chromaKeyMaxDistance(params.mode);
    const double d0 = clamp01(params.tolerance) * dMax;
    const double soft = clamp01(params.feather) * dMax;

    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < cols; ++x) {
            const double dr = pix(front.r(), y, x) - params.keyR;
            const double dg = pix(front.g(), y, x) - params.keyG;
            const double db = pix(front.b(), y, x) - params.keyB;

            double dist;
            if (params.mode == ChromaKeyMode::Sphere) {
                dist = std::sqrt(dr * dr + dg * dg + db * db);
            } else {
                dist = std::max(std::fabs(dr), std::max(std::fabs(dg), std::fabs(db)));
            }

            // w = peso do fundo: 1 dentro de D0, cai suavemente ate 0 em D0 + soft
            double w;
            if (dist <= d0) {
                w = 1.0;
            } else if (soft > 0.0 && dist < d0 + soft) {
                const double t = (dist - d0) / soft;
                w = 1.0 - t * t * (3.0 - 2.0 * t);   // smoothstep
            } else {
                w = 0.0;
            }
            if (w <= 0.0) continue;

            for (int c = 0; c < ColorImage::kChannels; ++c) {
                const double f = pix(front.channel(c), y, x);
                const double b = pix(bg.channel(c), y, x);
                front.channel(c).pixel(y, x) = clampByte(w * b + (1.0 - w) * f);
            }
        }
    }
}

bool sampleAverageColor(ColorImage& img, int row, int col, int radius, int& r, int& g, int& b) {
    if (row < 0 || col < 0 || row >= img.getRows() || col >= img.getCols()) return false;
    radius = std::max(0, radius);

    long long sr = 0, sg = 0, sb = 0, count = 0;
    for (int y = std::max(0, row - radius); y <= std::min(img.getRows() - 1, row + radius); ++y) {
        for (int x = std::max(0, col - radius); x <= std::min(img.getCols() - 1, col + radius); ++x) {
            sr += pix(img.r(), y, x);
            sg += pix(img.g(), y, x);
            sb += pix(img.b(), y, x);
            ++count;
        }
    }
    if (count == 0) return false;
    r = static_cast<int>((sr + count / 2) / count);
    g = static_cast<int>((sg + count / 2) / count);
    b = static_cast<int>((sb + count / 2) / count);
    return true;
}

ColorHistograms computeColorHistograms(ColorImage& img) {
    ColorHistograms h;
    h.r = computeHistogram(img.r(), 256);
    h.g = computeHistogram(img.g(), 256);
    h.b = computeHistogram(img.b(), 256);
    Image i = intensityPlane(img);
    h.i = computeHistogram(i, 256);
    return h;
}

void equalizeHistogramHSI(ColorImage& img, double saturationGain) {
    if (img.getRows() <= 0 || img.getCols() <= 0) return;

    // 1. (H, S, I) = RGB -> HSI
    HsiImage hsi = rgbToHsi(img);

    // 2. Iq = round(I * 255)   3. LUT[k] = round(255 * soma de p(j), j <= k)
    Image iq = hsiPlaneToImage(hsi, HsiPlane::I);
    std::vector<int> lut = equalizationMapping(computeHistogram(iq, 256));

    // 4. I' = LUT[Iq] / 255 (H e S nao mudam; S so muda se houver ganho)
    for (int y = 0; y < hsi.rows; ++y) {
        for (int x = 0; x < hsi.cols; ++x) {
            const size_t k = hsi.index(y, x);
            hsi.i[k] = static_cast<float>(lut[static_cast<size_t>(iq.pixel(y, x))] / 255.0);
            hsi.s[k] = static_cast<float>(clamp01(hsi.s[k] * saturationGain));
        }
    }

    // 5. HSI -> RGB
    img = hsiToRgb(hsi, img);
}

void adjustHsi(ColorImage& img, double deltaHue, double saturationFactor, double intensityFactor) {
    if (img.getRows() <= 0 || img.getCols() <= 0) return;

    HsiImage hsi = rgbToHsi(img);
    for (size_t k = 0; k < hsi.h.size(); ++k) {
        double h = std::fmod(static_cast<double>(hsi.h[k]) + deltaHue, 360.0);
        if (h < 0.0) h += 360.0;
        hsi.h[k] = static_cast<float>(h);
        hsi.s[k] = static_cast<float>(clamp01(hsi.s[k] * saturationFactor));
        hsi.i[k] = static_cast<float>(clamp01(hsi.i[k] * intensityFactor));
    }
    img = hsiToRgb(hsi, img);
}

void adjustChannels(ColorImage& img, int dR, int dG, int dB) {
    const int delta[3] = {dR, dG, dB};
    for (int c = 0; c < ColorImage::kChannels; ++c) {
        for (auto& row : img.channel(c).getData()) {
            for (int& v : row) {
                v = std::max(0, std::min(v + delta[c], 255));
            }
        }
    }
}

ColorImage gradientMagnitudeColor(ColorImage& img, BorderMode border, bool approximate, bool diZenzo) {
    const int rows = img.getRows();
    const int cols = img.getCols();
    Image out(rows, cols, 255, img.getBitsPerPixel(), img.getDpi());
    if (rows <= 0 || cols <= 0) return ColorImage(out);

    if (!diZenzo) {
        // Magnitude em cada canal, somadas
        for (int c = 0; c < ColorImage::kChannels; ++c) {
            Image m = gradientMagnitude(img.channel(c), border, approximate);
            for (int y = 0; y < rows; ++y) {
                for (int x = 0; x < cols; ++x) out.pixel(y, x) += m.pixel(y, x);
            }
        }
        return ColorImage(out);
    }

    // Di Zenzo: derivadas parciais (Sobel) de R, G e B
    Image gx[3] = {Image(0, 0), Image(0, 0), Image(0, 0)};
    Image gy[3] = {Image(0, 0), Image(0, 0), Image(0, 0)};
    for (int c = 0; c < ColorImage::kChannels; ++c) {
        gx[c] = sobelX(img.channel(c), border);
        gy[c] = sobelY(img.channel(c), border);
    }

    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < cols; ++x) {
            double gxx = 0.0, gyy = 0.0, gxy = 0.0;
            for (int c = 0; c < ColorImage::kChannels; ++c) {
                const double a = gx[c].pixel(y, x);
                const double b = gy[c].pixel(y, x);
                gxx += a * a;
                gyy += b * b;
                gxy += a * b;
            }
            // F(theta) maximo = sqrt{ 1/2 [ (gxx + gyy) + sqrt((gxx - gyy)^2 + 4 gxy^2) ] }
            // (valor de F no angulo theta = 1/2 atan2(2 gxy, gxx - gyy), Eqs. 6.7-7 e 6.7-8)
            const double root = std::sqrt((gxx - gyy) * (gxx - gyy) + 4.0 * gxy * gxy);
            const double f2 = 0.5 * ((gxx + gyy) + root);
            out.pixel(y, x) = static_cast<int>(std::llround(std::sqrt(std::max(0.0, f2))));
        }
    }
    return ColorImage(out);
}

ColorImage makeView(ColorImage& img, ColorView view) {
    switch (view) {
        case ColorView::R: return ColorImage(img.r());
        case ColorView::G: return ColorImage(img.g());
        case ColorView::B: return ColorImage(img.b());
        case ColorView::H:
        case ColorView::S:
        case ColorView::I: {
            if (img.getRows() <= 0 || img.getCols() <= 0) return img;
            HsiPlane plane = view == ColorView::H ? HsiPlane::H : (view == ColorView::S ? HsiPlane::S : HsiPlane::I);
            Image p = hsiPlaneToImage(rgbToHsi(img), plane);
            p.setDpi(img.getDpi());
            p.setBitsPerPixel(img.getBitsPerPixel());
            return ColorImage(p);
        }
        default: return img;
    }
}

}
