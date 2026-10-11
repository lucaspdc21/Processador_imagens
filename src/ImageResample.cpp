#include "ImageResample.hpp"
#include <algorithm>
#include <cmath>

namespace core {

static float getPixelClamped(const Image& img, int x, int y, int ch) {
    x = std::clamp(x, 0, img.getRows() - 1);
    y = std::clamp(y, 0, img.getCols() - 1);
    return img.at(x, y, ch);
}

static float sampleNearest(const Image& src, double x, double y, int ch) {
    int srcX = static_cast<int>(std::round(x));
    int srcY = static_cast<int>(std::round(y));
    return getPixelClamped(src, srcX, srcY, ch);
}

static float sampleBilinear(const Image& src, double x, double y, int ch) {
    int x0 = static_cast<int>(std::floor(x));
    int y0 = static_cast<int>(std::floor(y));
    int x1 = x0 + 1;
    int y1 = y0 + 1;
    double fx = x - x0;
    double fy = y - y0;

    double v00 = getPixelClamped(src, x0, y0, ch);
    double v10 = getPixelClamped(src, x1, y0, ch);
    double v01 = getPixelClamped(src, x0, y1, ch);
    double v11 = getPixelClamped(src, x1, y1, ch);

    double top = v00 * (1.0 - fx) + v10 * fx;
    double bottom = v01 * (1.0 - fx) + v11 * fx;
    double value = top * (1.0 - fy) + bottom * fy;

    return static_cast<float>(std::round(value));
}

static double cubicWeight(double t) {
    const double a = -0.5;
    t = std::abs(t);
    if (t <= 1.0) {
        return (a + 2.0) * t * t * t - (a + 3.0) * t * t + 1.0;
    }
    if (t < 2.0) {
        return a * t * t * t - 5.0 * a * t * t + 8.0 * a * t - 4.0 * a;
    }
    return 0.0;
}

static float sampleBicubic(const Image& src, double x, double y, int ch) {
    int x1 = static_cast<int>(std::floor(x));
    int y1 = static_cast<int>(std::floor(y));
    double fx = x - x1;
    double fy = y - y1;

    double result = 0.0;
    for (int m = -1; m <= 2; ++m) {
        double wx = cubicWeight(m - fx);
        for (int n = -1; n <= 2; ++n) {
            double wy = cubicWeight(n - fy);
            result += getPixelClamped(src, x1 + m, y1 + n, ch) * wx * wy;
        }
    }

    return static_cast<float>(std::round(std::clamp(result, 0.0, static_cast<double>(kMaxIntensity))));
}

void resample(Image& img, int newRows, int newCols, resampleType method) {
    if (newRows < 1) newRows = 1;
    if (newCols < 1) newCols = 1;

    int oldRows = img.getRows();
    int oldCols = img.getCols();
    int channels = img.getChannels();

    if (oldRows == 0 || oldCols == 0) {
        img = Image(newRows, newCols, img.getMaxIntensity(), img.getBitsPerPixel(), img.getDpi(), channels);
        return;
    }

    Image source = img;
    img = Image(newRows, newCols, source.getMaxIntensity(), source.getBitsPerPixel(), source.getDpi(), channels);

    for (int x = 0; x < newRows; ++x) {
        double srcX = (static_cast<double>(x) + 0.5) * oldRows / newRows - 0.5;
        for (int y = 0; y < newCols; ++y) {
            double srcY = (static_cast<double>(y) + 0.5) * oldCols / newCols - 0.5;

            for (int ch = 0; ch < channels; ++ch) {
                float value;
                switch (method) {
                    case resampleType::Bilinear:
                        value = sampleBilinear(source, srcX, srcY, ch);
                        break;
                    case resampleType::Bicubica:
                        value = sampleBicubic(source, srcX, srcY, ch);
                        break;
                    default:
                        value = sampleNearest(source, srcX, srcY, ch);
                        break;
                }
                img.at(x, y, ch) = value;
            }
        }
    }
}

}
