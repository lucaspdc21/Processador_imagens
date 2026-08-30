#pragma once
#include "Image.hpp"

namespace core {

    enum class resampleType{
        Nearest,
        Bilinear,
        Bicubica
    };

    void resample(Image& img, int newRows, int newCols, resampleType interpolacao = resampleType::Nearest);

}
