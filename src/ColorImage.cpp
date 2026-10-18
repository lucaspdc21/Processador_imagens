#include "ColorImage.hpp"
#include <algorithm>
#include <climits>
#include <cmath>

namespace core {

bool ColorImage::isGray() {
    auto& dr = planes[0].getData();
    auto& dg = planes[1].getData();
    auto& db = planes[2].getData();
    for (size_t y = 0; y < dr.size(); ++y) {
        for (size_t x = 0; x < dr[y].size(); ++x) {
            if (dr[y][x] != dg[y][x] || dr[y][x] != db[y][x]) return false;
        }
    }
    return true;
}

Image intensityPlane(ColorImage& img) {
    Image out(img.getRows(), img.getCols(), 255, img.getBitsPerPixel(), img.getDpi());
    auto& dr = img.r().getData();
    auto& dg = img.g().getData();
    auto& db = img.b().getData();
    auto& dout = out.getData();
    for (int y = 0; y < img.getRows(); ++y) {
        for (int x = 0; x < img.getCols(); ++x) {
            int r = std::max(0, std::min(dr[y][x], 255));
            int g = std::max(0, std::min(dg[y][x], 255));
            int b = std::max(0, std::min(db[y][x], 255));
            dout[y][x] = (r + g + b + 1) / 3;   // arredondamento de (r+g+b)/3
        }
    }
    return out;
}

ColorImage normalizeForDisplay(ColorImage& img) {
    ColorImage out = img;
    if (img.getRows() <= 0 || img.getCols() <= 0) return out;

    int mn = INT_MAX;
    int mx = INT_MIN;
    for (int c = 0; c < ColorImage::kChannels; ++c) {
        for (const auto& row : out.channel(c).getData()) {
            for (int v : row) {
                mn = std::min(mn, v);
                mx = std::max(mx, v);
            }
        }
    }

    for (int c = 0; c < ColorImage::kChannels; ++c) {
        for (auto& row : out.channel(c).getData()) {
            for (int& v : row) {
                if (mx > mn) {
                    v = static_cast<int>(std::llround((static_cast<double>(v) - mn) * 255.0 /
                                                      (static_cast<double>(mx) - mn)));
                } else {
                    v = std::max(0, std::min(v, 255));
                }
            }
        }
    }
    return out;
}

}
