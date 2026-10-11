#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include "ImageLoad.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <vector>

namespace core {

namespace {

    std::string lowerExtension(const std::string& path) {
        size_t slash = path.find_last_of("/\\");
        size_t dot = path.find_last_of('.');
        if (dot == std::string::npos || (slash != std::string::npos && dot < slash)) {
            return "";
        }
        std::string ext = path.substr(dot);
        std::transform(ext.begin(), ext.end(), ext.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return ext;
    }

}

bool loadImage(const std::string& path, Image& outImage) {
    int width = 0, height = 0, channelsInFile = 0;

    // desired_channels = 0: mantem os canais do arquivo.
    unsigned char* pixels = stbi_load(path.c_str(), &width, &height, &channelsInFile, 0);
    if (!pixels) {
        return false;
    }

    // 1 = cinza, 2 = cinza + alfa, 3 = RGB, 4 = RGB + alfa. O alfa e descartado.
    int outChannels = (channelsInFile >= 3) ? 3 : 1;

    // BMP nao tem formato de 1 canal: uma imagem cinza salva como BMP volta como RGB com
    // R = G = B. Nesse caso (so em BMP) a imagem volta a ter 1 canal.
    if (outChannels == 3 && lowerExtension(path) == ".bmp") {
        bool allGray = true;
        for (size_t i = 0; i < static_cast<size_t>(width) * height && allGray; ++i) {
            const unsigned char* px = pixels + i * channelsInFile;
            allGray = (px[0] == px[1] && px[1] == px[2]);
        }
        if (allGray) outChannels = 1;
    }

    Image loaded(height, width, kMaxIntensity, 8, 72.0, outChannels);
    for (int row = 0; row < height; ++row) {
        for (int col = 0; col < width; ++col) {
            size_t idx = (static_cast<size_t>(row) * width + col) * channelsInFile;
            for (int ch = 0; ch < outChannels; ++ch) {
                loaded.at(row, col, ch) = static_cast<float>(pixels[idx + ch]);
            }
        }
    }

    stbi_image_free(pixels);
    outImage = loaded;
    return true;
}

bool hasSupportedSaveExtension(const std::string& path) {
    std::string ext = lowerExtension(path);
    return ext == ".png" || ext == ".bmp" || ext == ".jpg" || ext == ".jpeg";
}

bool isJpegPath(const std::string& path) {
    std::string ext = lowerExtension(path);
    return ext == ".jpg" || ext == ".jpeg";
}

bool saveImage(const std::string& path, const Image& image) {
    int height = image.getRows();
    int width = image.getCols();
    int channels = image.getChannels();
    if (height <= 0 || width <= 0 || (channels != 1 && channels != 3)) {
        return false;
    }

    std::vector<unsigned char> pixels(image.sampleCount());
    const std::vector<float>& samples = image.samples();
    for (size_t i = 0; i < samples.size(); ++i) {
        long v = std::lround(samples[i]);
        v = std::max(0L, std::min(v, static_cast<long>(kMaxIntensity)));
        pixels[i] = static_cast<unsigned char>(v);
    }

    std::string ext = lowerExtension(path);
    int result = 0;
    if (ext == ".bmp") {
        result = stbi_write_bmp(path.c_str(), width, height, channels, pixels.data());
    } else if (ext == ".jpg" || ext == ".jpeg") {
        result = stbi_write_jpg(path.c_str(), width, height, channels, pixels.data(), 95);
    } else {
        result = stbi_write_png(path.c_str(), width, height, channels, pixels.data(),
                                width * channels);
    }
    return result != 0;
}

}
