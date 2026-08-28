#pragma once
#include "Image.hpp"
#include <string>

namespace core {

bool loadImage(std::string& path, Image& outImage);
bool saveImagePNG(const std::string& path, const Image& image);

}
