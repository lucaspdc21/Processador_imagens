#pragma once
#include "Image.hpp"

namespace core {

    struct SpatialMetrics {
        double widthInches = 0.0;
        double heightInches = 0.0;
        double linePairsPerMm = 0.0; 
    };

    SpatialMetrics computeSpatialMetrics(const Image& img);

    struct StorageMetrics {
        long long bits = 0;
        double bytes = 0.0;
        double kilobytes = 0.0;
        double megabytes = 0.0;
    };

    StorageMetrics computeStorageMetrics(const Image& img);

}
