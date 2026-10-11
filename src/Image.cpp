#include "Image.hpp"
#include <algorithm>

namespace core {

Image::Image(int rows, int cols, int maxIntensity, int bitsPerPixel, double dpi, int channels)
    : rows(std::max(0, rows)), cols(std::max(0, cols)), maxIntensity(maxIntensity),
      bitsPerPixel(bitsPerPixel), dpi(dpi), channels(std::max(1, channels)) {
    data.assign(static_cast<size_t>(this->rows) * this->cols * this->channels, 0.0f);
}

void Image::fill(float value) {
    std::fill(data.begin(), data.end(), value);
}

void Image::clampToRange() {
    const float hi = static_cast<float>(maxIntensity);
    for (float& v : data) {
        v = std::max(0.0f, std::min(v, hi));
    }
}

}
