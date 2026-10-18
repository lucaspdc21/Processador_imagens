#pragma once
#include "Image.hpp"
#include "ColorImage.hpp"

namespace core {

    struct ImageStats {
        int minValue = 0;
        int maxValue = 0;
        int dynamicRange = 0;
        double contrast = 0.0;  
    };

    ImageStats computeStats(Image& img);

    // Minimo e maximo considerando os tres planos
    ImageStats computeStats(ColorImage& img);

}
