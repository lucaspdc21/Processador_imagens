#include "ImageIntensity.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace core {

namespace {

    inline int clampByte(double v) {
        if (v < 0.0) return 0;
        if (v > 255.0) return 255;
        return static_cast<int>(std::lround(v));
    }

    // Avalia a reta definida por dois pontos de controle (r1,s1) e (r2,s2),
    // usada pelo alargamento de contraste (funcao linear por partes).
    double pieceLinear(double r, int r1, int s1, int r2, int s2) {
        if (r < r1) {
            return (r1 <= 0) ? static_cast<double>(s1) : (static_cast<double>(s1) / r1) * r;
        } else if (r < r2) {
            double denom = static_cast<double>(r2 - r1);
            if (denom == 0.0) return s2;
            return (static_cast<double>(s2 - s1) / denom) * (r - r1) + s1;
        } else {
            double denom = static_cast<double>(255 - r2);
            if (denom == 0.0) return s2;
            return (static_cast<double>(255 - s2) / denom) * (r - r2) + s2;
        }
    }

}

void threshold(Image& img, int limiar) {
    for (int row = 0; row < img.getRows(); ++row) {
        for (int col = 0; col < img.getCols(); ++col) {
            int r = img.pixel(row, col);
            img.pixel(row, col) = (r < limiar) ? 0 : 255;
        }
    }
}

void negative(Image& img) {
    for (int row = 0; row < img.getRows(); ++row) {
        for (int col = 0; col < img.getCols(); ++col) {
            int r = std::max(0, std::min(img.pixel(row, col), 255));
            img.pixel(row, col) = 255 - r;
        }
    }
}

double computeAutoLogConstant(Image& img) {
    int maxVal = 0;
    bool any = false;
    for (auto& row : img.getData()) {
        for (int v : row) {
            if (!any || v > maxVal) { maxVal = v; any = true; }
        }
    }
    if (maxVal <= 0) return 1.0;
    return 255.0 / std::log(1.0 + static_cast<double>(maxVal));
}

void logTransform(Image& img, double c, bool autoConstant) {
    double constant = autoConstant ? computeAutoLogConstant(img) : c;
    for (int row = 0; row < img.getRows(); ++row) {
        for (int col = 0; col < img.getCols(); ++col) {
            int r = std::max(0, std::min(img.pixel(row, col), 255));
            double s = constant * std::log(1.0 + static_cast<double>(r));
            img.pixel(row, col) = clampByte(s);
        }
    }
}

void gammaCorrection(Image& img, double gamma, double c) {
    for (int row = 0; row < img.getRows(); ++row) {
        for (int col = 0; col < img.getCols(); ++col) {
            int r = std::max(0, std::min(img.pixel(row, col), 255));
            double rn = static_cast<double>(r) / 255.0;
            double sn = c * std::pow(rn, gamma);
            img.pixel(row, col) = clampByte(sn * 255.0);
        }
    }
}

void contrastStretch(Image& img, int r1, int s1, int r2, int s2) {
    r1 = std::max(0, std::min(r1, 255));
    r2 = std::max(0, std::min(r2, 255));
    s1 = std::max(0, std::min(s1, 255));
    s2 = std::max(0, std::min(s2, 255));
    if (r1 > r2) std::swap(r1, r2); // mantem os pontos em ordem crescente

    for (int row = 0; row < img.getRows(); ++row) {
        for (int col = 0; col < img.getCols(); ++col) {
            int r = std::max(0, std::min(img.pixel(row, col), 255));
            double s = pieceLinear(static_cast<double>(r), r1, s1, r2, s2);
            img.pixel(row, col) = clampByte(s);
        }
    }
}

void intensitySlice(Image& img, int rMin, int rMax, int highlightValue,
                     bool preserveBackground, int backgroundValue) {
    if (rMin > rMax) std::swap(rMin, rMax);
    highlightValue = std::max(0, std::min(highlightValue, 255));
    backgroundValue = std::max(0, std::min(backgroundValue, 255));

    for (int row = 0; row < img.getRows(); ++row) {
        for (int col = 0; col < img.getCols(); ++col) {
            int r = std::max(0, std::min(img.pixel(row, col), 255));
            if (r >= rMin && r <= rMax) {
                img.pixel(row, col) = highlightValue;
            } else if (!preserveBackground) {
                img.pixel(row, col) = backgroundValue;
            }
        }
    }
}

void bitPlaneSlice(Image& img, int plane) {
    plane = std::max(1, std::min(plane, 8));
    int mask = 1 << (plane - 1);
    for (int row = 0; row < img.getRows(); ++row) {
        for (int col = 0; col < img.getCols(); ++col) {
            int r = std::max(0, std::min(img.pixel(row, col), 255));
            img.pixel(row, col) = (r & mask) ? 255 : 0;
        }
    }
}

long long steganographyCapacityBytes(Image& img) {
    long long totalPixels = static_cast<long long>(img.getRows()) * img.getCols();
    long long availableBits = totalPixels - 32; // 32 bits reservados para o cabecalho de tamanho
    if (availableBits <= 0) return 0;
    return availableBits / 8;
}

bool embedMessageLSB(Image& img, const std::string& message) {
    long long totalPixels = static_cast<long long>(img.getRows()) * img.getCols();
    long long totalBitsNeeded = 32 + static_cast<long long>(message.size()) * 8;
    if (message.empty() || totalBitsNeeded > totalPixels) {
        return false; // mensagem vazia ou nao cabe na imagem
    }

    auto& data = img.getData();
    int cols = img.getCols();
    uint32_t length = static_cast<uint32_t>(message.size());

    long long bitIndex = 0;
    auto writeBit = [&](int bit) {
        int row = static_cast<int>(bitIndex / cols);
        int col = static_cast<int>(bitIndex % cols);
        int value = std::max(0, std::min(data[row][col], 255));
        value = (value & ~1) | (bit & 1); // substitui apenas o bit menos significativo
        data[row][col] = value;
        ++bitIndex;
    };

    // Cabecalho: 32 bits com o tamanho da mensagem em bytes (MSB primeiro)
    for (int i = 31; i >= 0; --i) {
        writeBit((length >> i) & 1);
    }

    // Corpo: cada byte da mensagem, bit mais significativo primeiro
    for (unsigned char byte : message) {
        for (int i = 7; i >= 0; --i) {
            writeBit((byte >> i) & 1);
        }
    }

    return true;
}

std::string extractMessageLSB(Image& img) {
    long long totalPixels = static_cast<long long>(img.getRows()) * img.getCols();
    if (totalPixels < 32) return "";

    auto& data = img.getData();
    int cols = img.getCols();

    long long bitIndex = 0;
    auto readBit = [&]() -> int {
        int row = static_cast<int>(bitIndex / cols);
        int col = static_cast<int>(bitIndex % cols);
        int value = std::max(0, std::min(data[row][col], 255));
        ++bitIndex;
        return value & 1;
    };

    uint32_t length = 0;
    for (int i = 0; i < 32; ++i) {
        length = (length << 1) | static_cast<uint32_t>(readBit());
    }

    long long bitsAvailable = totalPixels - 32;
    long long bitsNeeded = static_cast<long long>(length) * 8;
    if (length == 0 || bitsNeeded > bitsAvailable) {
        return ""; // nenhum cabecalho valido encontrado (imagem provavelmente nao tem mensagem)
    }

    std::string message;
    message.reserve(length);
    for (uint32_t b = 0; b < length; ++b) {
        unsigned char byte = 0;
        for (int i = 0; i < 8; ++i) {
            byte = static_cast<unsigned char>((byte << 1) | readBit());
        }
        message.push_back(static_cast<char>(byte));
    }
    return message;
}

}
