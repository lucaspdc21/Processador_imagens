#pragma once
#include "Image.hpp"
#include "ColorImage.hpp"

namespace core {

    struct SpatialMetrics {
        double widthInches = 0.0;
        double heightInches = 0.0;
        double linePairsPerMm = 0.0; 
    };

    SpatialMetrics computeSpatialMetrics(Image& img);

    struct StorageMetrics {
        long long bits = 0;
        double bytes = 0.0;
        double kilobytes = 0.0;
        double megabytes = 0.0;
    };

    StorageMetrics computeStorageMetrics(Image& img);

    // Imagem colorida: k bits por canal, 3 canais
    SpatialMetrics computeSpatialMetrics(ColorImage& img);
    StorageMetrics computeStorageMetrics(ColorImage& img);

}
