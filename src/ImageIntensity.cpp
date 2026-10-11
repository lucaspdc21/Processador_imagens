#include "ImageIntensity.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace core {

namespace {

    constexpr double kMax = static_cast<double>(kMaxIntensity);

    // Limita a 0..255 e arredonda para o inteiro mais proximo.
    inline float clampByte(double v) {
        if (v < 0.0) return 0.0f;
        if (v > kMax) return static_cast<float>(kMaxIntensity);
        return static_cast<float>(std::lround(v));
    }

    // Le uma amostra como inteiro de 0 a 255.
    inline int readByte(float v) {
        long r = std::lround(v);
        return static_cast<int>(std::max(0L, std::min(r, static_cast<long>(kMaxIntensity))));
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
            double denom = static_cast<double>(kMaxIntensity - r2);
            if (denom == 0.0) return s2;
            return (static_cast<double>(kMaxIntensity - s2) / denom) * (r - r2) + s2;
        }
    }

}

void threshold(Image& img, int limiar) {
    for (float& sample : img.samples()) {
        int r = readByte(sample);
        sample = (r < limiar) ? 0.0f : static_cast<float>(kMaxIntensity);
    }
}

void negative(Image& img) {
    for (float& sample : img.samples()) {
        sample = static_cast<float>(kMaxIntensity - readByte(sample));
    }
}

void brightness(Image& img, int delta) {
    for (float& sample : img.samples()) {
        sample = clampByte(static_cast<double>(readByte(sample)) + delta);
    }
}

double computeAutoLogConstant(const Image& img) {
    int maxVal = 0;
    bool any = false;
    for (float sample : img.samples()) {
        int v = readByte(sample);
        if (!any || v > maxVal) { maxVal = v; any = true; }
    }
    if (maxVal <= 0) return 1.0;
    return kMax / std::log(1.0 + static_cast<double>(maxVal));
}

void logTransform(Image& img, double c, bool autoConstant) {
    double constant = autoConstant ? computeAutoLogConstant(img) : c;
    for (float& sample : img.samples()) {
        int r = readByte(sample);
        sample = clampByte(constant * std::log(1.0 + static_cast<double>(r)));
    }
}

void gammaCorrection(Image& img, double gamma, double c) {
    for (float& sample : img.samples()) {
        int r = readByte(sample);
        double rn = static_cast<double>(r) / kMax;
        double sn = c * std::pow(rn, gamma);
        sample = clampByte(sn * kMax);
    }
}

void contrastStretch(Image& img, int r1, int s1, int r2, int s2) {
    r1 = std::max(0, std::min(r1, kMaxIntensity));
    r2 = std::max(0, std::min(r2, kMaxIntensity));
    s1 = std::max(0, std::min(s1, kMaxIntensity));
    s2 = std::max(0, std::min(s2, kMaxIntensity));
    if (r1 > r2) std::swap(r1, r2); // mantem os pontos em ordem crescente

    for (float& sample : img.samples()) {
        int r = readByte(sample);
        sample = clampByte(pieceLinear(static_cast<double>(r), r1, s1, r2, s2));
    }
}

void intensitySlice(Image& img, int rMin, int rMax, int highlightValue,
                     bool preserveBackground, int backgroundValue) {
    if (rMin > rMax) std::swap(rMin, rMax);
    highlightValue = std::max(0, std::min(highlightValue, kMaxIntensity));
    backgroundValue = std::max(0, std::min(backgroundValue, kMaxIntensity));

    for (float& sample : img.samples()) {
        int r = readByte(sample);
        if (r >= rMin && r <= rMax) {
            sample = static_cast<float>(highlightValue);
        } else if (!preserveBackground) {
            sample = static_cast<float>(backgroundValue);
        }
    }
}

void bitPlaneSlice(Image& img, int plane) {
    plane = std::max(1, std::min(plane, 8));
    int mask = 1 << (plane - 1);
    for (float& sample : img.samples()) {
        int r = readByte(sample);
        sample = (r & mask) ? static_cast<float>(kMaxIntensity) : 0.0f;
    }
}

long long steganographyCapacityBytes(const Image& img) {
    long long totalSamples = static_cast<long long>(img.sampleCount());
    long long availableBits = totalSamples - 32; // 32 bits reservados para o cabecalho de tamanho
    if (availableBits <= 0) return 0;
    return availableBits / 8;
}

bool embedMessageLSB(Image& img, const std::string& message) {
    long long totalSamples = static_cast<long long>(img.sampleCount());
    long long totalBitsNeeded = 32 + static_cast<long long>(message.size()) * 8;
    if (message.empty() || totalBitsNeeded > totalSamples) {
        return false; // mensagem vazia ou nao cabe na imagem
    }

    std::vector<float>& data = img.samples();
    uint32_t length = static_cast<uint32_t>(message.size());

    size_t bitIndex = 0;
    auto writeBit = [&](int bit) {
        int value = readByte(data[bitIndex]);
        value = (value & ~1) | (bit & 1); // substitui apenas o bit menos significativo
        data[bitIndex] = static_cast<float>(value);
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

std::string extractMessageLSB(const Image& img) {
    long long totalSamples = static_cast<long long>(img.sampleCount());
    if (totalSamples < 32) return "";

    const std::vector<float>& data = img.samples();

    size_t bitIndex = 0;
    auto readBit = [&]() -> int {
        int value = readByte(data[bitIndex]);
        ++bitIndex;
        return value & 1;
    };

    uint32_t length = 0;
    for (int i = 0; i < 32; ++i) {
        length = (length << 1) | static_cast<uint32_t>(readBit());
    }

    long long bitsAvailable = totalSamples - 32;
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
