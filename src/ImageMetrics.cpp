#include "ImageMetrics.hpp"

namespace core {

SpatialMetrics computeSpatialMetrics(Image& img) {
    SpatialMetrics m;
    double dpi = img.getDpi();
    if (dpi <= 0) return m;

    m.widthInches = static_cast<double>(img.getCols()) / dpi;
    m.heightInches = static_cast<double>(img.getRows()) / dpi;

    double dotsPerMm = dpi / 25.4;
    m.linePairsPerMm = dotsPerMm / 2.0;

    return m;
}

StorageMetrics computeStorageMetrics(Image& img) {
    StorageMetrics m;
    m.bits = static_cast<long long>(img.getRows()) *
             static_cast<long long>(img.getCols()) *
             static_cast<long long>(img.getBitsPerPixel());
    m.bytes = static_cast<double>(m.bits) / 8.0;
    m.kilobytes = m.bytes / 1024.0;
    m.megabytes = m.kilobytes / 1024.0;
    return m;
}

}
