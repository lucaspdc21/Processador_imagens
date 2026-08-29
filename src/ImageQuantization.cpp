#include "ImageQuantization.hpp"
#include <algorithm>

namespace core {

long long levelsForBits(int k) {
    if (k < 1) k = 1;
    if (k > 8) k = 8;
    return 1LL << k; 
}

void quantize(Image& img, int k) {
    if (k < 1) k = 1;
    if (k > 8) k = 8;

    int levels = static_cast<int>(levelsForBits(k));
    int step = 256 / levels;

    for (int x = 0; x < img.getRows(); ++x) {
        for (int y = 0; y < img.getCols(); ++y) {
            int clamped = std::max(0, std::min(img.pixel(x, y), 255));
            int q = clamped / step;
            if (q >= levels) q = levels - 1;
            int reconstructed = std::min(q * step + step / 2, 255);
            img.pixel(x, y) = reconstructed;
        }
    }

    img.setBitsPerPixel(k);
}

}
