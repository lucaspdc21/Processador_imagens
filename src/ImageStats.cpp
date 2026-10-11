#include "ImageStats.hpp"
#include <cmath>

namespace core {

ImageStats computeStats(const Image& img) {
    ImageStats stats;
    bool any = false;

    // Considera todas as amostras (todos os canais).
    for (float sample : img.samples()) {
        int v = static_cast<int>(std::lround(sample));
        if (!any) {
            stats.minValue = v;
            stats.maxValue = v;
            any = true;
        } else {
            if (v < stats.minValue) stats.minValue = v;
            if (v > stats.maxValue) stats.maxValue = v;
        }
    }

    stats.dynamicRange = stats.maxValue - stats.minValue;

    int sum = stats.maxValue + stats.minValue;
    stats.contrast = (sum == 0) ? 0.0
        : static_cast<double>(stats.dynamicRange) / static_cast<double>(sum);

    return stats;
}

}
