#include "Image.hpp"

namespace core {
Image::Image(int rows, int cols, int maxIntensity)
    : rows(rows), cols(cols), maxIntensity(maxIntensity){
        data = std::vector<std::vector<int>>(rows, std::vector<int>(cols,0));
}

int& Image::pixel(int x, int y){
    return data[x][y];
}

void Image::fill(int value) {
    for (auto& row : data) {
        std::fill(row.begin(), row.end(), value);
    }
}

void Image::clampToRange() {
    for (auto& row : data) {
        for (auto& v : row) {
            v = std::max(0, std::min(v, maxIntensity));
        }
    }
}


}