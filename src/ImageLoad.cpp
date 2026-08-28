#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include "ImageLoad.hpp"
#include <vector>

namespace core{
    bool loadImage(std::string& path, Image& outImage){
    int width = 0, height = 0, channelsInFile = 0;

    unsigned char* pixels = stbi_load(path.c_str(), &width, &height, &channelsInFile, 3);
    if (!pixels) {
        return false;
    }

    outImage = Image(height, width, 255);

    for (int row = 0; row < height; ++row) {
        for (int col = 0; col < width; ++col) {
            size_t idx = (static_cast<size_t>(row) * width + col) * 3;
            unsigned char r = pixels[idx + 0];
            unsigned char g = pixels[idx + 1];
            unsigned char b = pixels[idx + 2];

            int gray = static_cast<int>(0.299 * r + 0.587 * g + 0.114 * b);
            outImage.pixel(row, col) = gray;
        }
    }

    stbi_image_free(pixels);
    return true;

    }
}