#pragma once
#include "Image.hpp"
#include <string>

namespace core {

bool loadImage(std::string& path, Image& outImage);
bool saveImagePNG(std::string& path, Image& image);

}
