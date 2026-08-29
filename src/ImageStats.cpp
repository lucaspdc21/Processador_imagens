#include "ImageStats.hpp"

namespace core {

ImageStats computeStats(Image& img) {
    ImageStats stats;
    bool any = false;

    for (auto& row : img.getData()) {
        for (int v : row) {
            if (!any) {
                stats.minValue = v;
                stats.maxValue = v;
                any = true;
            } else {
                if (v < stats.minValue) stats.minValue = v;
                if (v > stats.maxValue) stats.maxValue = v;
            }
        }
    }

    stats.dynamicRange = stats.maxValue - stats.minValue;

    int sum = stats.maxValue + stats.minValue;
    stats.contrast = (sum == 0) ? 0.0
        : static_cast<double>(stats.dynamicRange) / static_cast<double>(sum);

    return stats;
}

}
