#include "ImageQuantization.hpp"
#include <algorithm>
#include <cmath>

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

    for (float& sample : img.samples()) {
        int clamped = std::max(0, std::min(static_cast<int>(std::lround(sample)), kMaxIntensity));
        int q = clamped / step;
        if (q >= levels) q = levels - 1;
        int reconstructed = std::min(q * step + step / 2, kMaxIntensity);
        sample = static_cast<float>(reconstructed);
    }

    img.setBitsPerPixel(k);
}

}
