#include "ImageResample.hpp"
#include <algorithm>
#include <cmath>

namespace core {

void resample(Image& img, int newRows, int newCols) {
    if (newRows < 1) newRows = 1;
    if (newCols < 1) newCols = 1;

    int oldRows = img.getRows();
    int oldCols = img.getCols();

    if (oldRows == 0 || oldCols == 0) {
        img = Image(newRows, newCols, img.getMaxIntensity(), img.getBitsPerPixel(), img.getDpi());
        return;
    }

    Image source = img; 
    img = Image(newRows, newCols, source.getMaxIntensity(), source.getBitsPerPixel(), source.getDpi());

    for (int x = 0; x < newRows; ++x) {
        double srcXf = (static_cast<double>(x) + 0.5) * oldRows / newRows - 0.5;
        int srcX = std::clamp(static_cast<int>(std::round(srcXf)), 0, oldRows - 1);
        for (int y = 0; y < newCols; ++y) {
            double srcYf = (static_cast<double>(y) + 0.5) * oldCols / newCols - 0.5;
            int srcY = std::clamp(static_cast<int>(std::round(srcYf)), 0, oldCols - 1);
            img.pixel(x, y) = source.pixel(srcX, srcY);
        }
    }
}

}
