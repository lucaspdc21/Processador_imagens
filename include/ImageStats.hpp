#pragma once
#include "Image.hpp"

namespace core {

    struct ImageStats {
        int minValue = 0;
        int maxValue = 0;
        int dynamicRange = 0;
        double contrast = 0.0;  
    };

    ImageStats computeStats(const Image& img);

}
