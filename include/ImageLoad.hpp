#pragma once
#include "Image.hpp"
#include "ColorImage.hpp"
#include <string>

namespace core {

bool loadImage(std::string& path, Image& outImage);
bool saveImagePNG(std::string& path, Image& image);

// Versoes coloridas: carregam / gravam R, G e B (imagens em cinza viram R = G = B)
bool loadImage(std::string& path, ColorImage& outImage);
bool saveImagePNG(std::string& path, ColorImage& image);

}
