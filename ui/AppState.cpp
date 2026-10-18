#include "AppState.hpp"
#include "ImageLoad.hpp"
#include "ImageQuantization.hpp"
#include "ImageResample.hpp"
#include "ImageIntensity.hpp"
#include "ImageConvolution.hpp"
#include "ImageSpatialFilters.hpp"
#include "ImageStats.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace ui {

bool AppState::loadFromPath(std::string& path) {
    std::string pathCopy = path;
    core::ColorImage loaded;
    if (!core::loadImage(pathCopy, loaded)) {
        statusMessage = "Falha ao carregar a imagem: " + pathCopy;
        return false;
    }

    originalImage = loaded;
    workingImage = loaded;
    hasImage = true;
    bitsPerPixel = workingImage.getBitsPerPixel();
    dpiValue = static_cast<float>(workingImage.getDpi());
    resamplePercent = 100;
    viewChoice = 0;
    chromaPicking = false;
    statusMessage = "Imagem carregada: " + pathCopy;
    return true;
}

bool AppState::saveToPath(std::string& path) {
    if (!hasImage) return false;
    if (core::saveImagePNG(path, workingImage)) {
        statusMessage = "Imagem salva em: " + path;
        return true;
    }
    statusMessage = "Falha ao salvar a imagem.";
    return false;
}

void AppState::applyQuantization() {
    if (!hasImage) return;
    core::applyToPlanes(workingImage, core::ColorTarget::ChannelsRGB,
                        [&](core::Image& p) { core::quantize(p, bitsPerPixel); });
    statusMessage = "Quantização aplicada (posterização).";
}

void AppState::applyResample() {
    if (!hasImage) return;

    int newRows = std::max(1, originalImage.getRows() * resamplePercent / 100);
    int newCols = std::max(1, originalImage.getCols() * resamplePercent / 100);

    // Parte sempre da original para nao acumular perdas de chamadas anteriores
    workingImage = originalImage;
    core::applyToPlanes(workingImage, core::ColorTarget::ChannelsRGB, [&](core::Image& p) {
        core::quantize(p, bitsPerPixel);
        core::resample(p, newRows, newCols, interpolationMethod);
    });
    if (resamplePercent < 100) {
        // Amplia de volta ao tamanho original para tornar visivel o efeito de
        // baixa resolucao espacial (pixelizacao)
        //core::resample(workingImage, originalImage.getRows(), originalImage.getCols(), interpolationMethod);
        statusMessage = "Reamostragem aplicada (efeito de baixa resolução espacial).";
    } else {
        statusMessage = "Reamostragem aplicada (imagem ampliada por interpolação).";
    }
}

void AppState::applyDpi(float newDpi) {
    if (newDpi < 1.0f) newDpi = 1.0f;
    dpiValue = newDpi;
    workingImage.setDpi(newDpi);
    originalImage.setDpi(newDpi);
}

void AppState::restoreOriginal() {
    if (!hasImage) return;
    workingImage = originalImage;
    bitsPerPixel = workingImage.getBitsPerPixel();
    resamplePercent = 100;
    chromaPicking = false;
    statusMessage = "Imagem restaurada ao estado original.";
}

void AppState::applyNegative() {
    if (!hasImage) return;
    // O negativo de uma imagem colorida e o complemento de cor, calculado em
    // RGB canal a canal (nao tem equivalente exato no HSI)
    core::applyToPlanes(workingImage, core::ColorTarget::ChannelsRGB, [](core::Image& p) { core::negative(p); });
    statusMessage = "Negativo da imagem aplicado.";
}

void AppState::applyBrightness() {
    if (!hasImage) return;
    core::applyToPlanes(workingImage, colorTarget(),
                        [&](core::Image& p) { core::brightness(p, static_cast<double>(brightnessFactor)); });
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.2f", brightnessFactor);
    statusMessage = std::string("Brilho aplicado (k = ") + buf + ").";
}

void AppState::applyThreshold() {
    if (!hasImage) return;
    core::applyToPlanes(workingImage, colorTarget(), [&](core::Image& p) { core::threshold(p, thresholdValue); });
    statusMessage = "Limiarização aplicada (m = " + std::to_string(thresholdValue) + ").";
}

void AppState::applyLogTransform() {
    if (!hasImage) return;
    // Constante automatica calculada com o maior valor dos 3 planos, para que
    // R, G e B usem a mesma funcao (senao as cores mudariam)
    double c = static_cast<double>(logConstant);
    if (logAutoConstant) {
        int maxVal = core::computeStats(workingImage).maxValue;
        c = (maxVal <= 0) ? 1.0 : 255.0 / std::log(1.0 + static_cast<double>(maxVal));
    }
    core::applyToPlanes(workingImage, colorTarget(), [&](core::Image& p) { core::logTransform(p, c, false); });
    statusMessage = "Transformação logarítmica aplicada.";
}

void AppState::applyGammaCorrection() {
    if (!hasImage) return;
    core::applyToPlanes(workingImage, colorTarget(), [&](core::Image& p) {
        core::gammaCorrection(p, static_cast<double>(gammaValue), static_cast<double>(gammaConstant));
    });
    statusMessage = "Correção de gama aplicada (gama = " + std::to_string(gammaValue) + ").";
}

void AppState::applyContrastStretch() {
    if (!hasImage) return;
    core::applyToPlanes(workingImage, colorTarget(), [&](core::Image& p) {
        core::contrastStretch(p, stretchR1, stretchS1, stretchR2, stretchS2);
    });
    statusMessage = "Alargamento de contraste aplicado.";
}

void AppState::applyIntensitySlice() {
    if (!hasImage) return;
    core::applyToPlanes(workingImage, colorTarget(), [&](core::Image& p) {
        core::intensitySlice(p, sliceMin, sliceMax, sliceHighlight, slicePreserveBackground);
    });
    statusMessage = "Fatiamento de níveis de intensidade aplicado.";
}

void AppState::applyBitPlaneSlice() {
    if (!hasImage) return;
    core::applyToPlanes(workingImage, colorTarget(), [&](core::Image& p) { core::bitPlaneSlice(p, bitPlaneSelected); });
    statusMessage = "Plano de bits " + std::to_string(bitPlaneSelected) + " extraído.";
}

void AppState::setKernelSize(int newSize) {
    if (!core::isValidKernelSize(newSize)) return;

    const size_t oldCount = static_cast<size_t>(kernelSize) * static_cast<size_t>(kernelSize);
    const bool oldIsConsistent = core::isValidKernelSize(kernelSize) && kernelValues.size() == oldCount;
    if (newSize == kernelSize && oldIsConsistent) return;

    std::vector<float> resized(static_cast<size_t>(newSize) * static_cast<size_t>(newSize), 0.0f);
    if (oldIsConsistent) {
        const int oldA = kernelSize / 2;
        const int newA = newSize / 2;
        const int common = std::min(oldA, newA);
        for (int s = -common; s <= common; ++s) {
            for (int t = -common; t <= common; ++t) {
                resized[static_cast<size_t>((s + newA) * newSize + (t + newA))] =
                    kernelValues[static_cast<size_t>((s + oldA) * kernelSize + (t + oldA))];
            }
        }
    }
    kernelValues = resized;
    kernelSize = newSize;
}

void AppState::applyConvolution() {
    if (!hasImage) return;

    const size_t count = static_cast<size_t>(kernelSize) * static_cast<size_t>(kernelSize);
    if (!core::isValidKernelSize(kernelSize) || kernelValues.size() != count) {
        statusMessage = "Kernel inválido: o tamanho deve ser ímpar.";
        return;
    }

    core::Kernel kernel(kernelSize);
    for (size_t i = 0; i < count; ++i) {
        kernel.weights[i] = static_cast<double>(kernelValues[i]);
    }

    if (kernelDivideBySum) {
        double sum = core::kernelSum(kernel);
        if (std::fabs(sum) < 1e-12) {
            statusMessage = "A soma dos coeficientes é zero: não dá para dividir por ela.";
            return;
        }
        for (double& w : kernel.weights) w /= sum;
    }

    core::FilterOperation operation = (filterOperation == 1) ? core::FilterOperation::Correlation
                                                              : core::FilterOperation::Convolution;
    core::BorderMode border = (filterBorder == 1) ? core::BorderMode::Replicate
                                                  : core::BorderMode::Zero;

    // Filtro linear: equivalente por canal. Com 'manter valores' o resultado
    // sai de [0, 255] e so faz sentido por canal RGB.
    core::ColorTarget target = (filterResultMode == 0) ? colorTarget() : core::ColorTarget::ChannelsRGB;
    core::applyToPlanes(workingImage, target, [&](core::Image& p) {
        core::applyFilter(p, kernel, operation, border, filterResultMode == 0);
    });

    statusMessage = "Filtro " + std::to_string(kernelSize) + "x" + std::to_string(kernelSize) +
                    (filterOperation == 1 ? " aplicado (correlação)." : " aplicado (convolução).");
}

namespace {

    void DecodeLaplacian(int choice, core::LaplacianKind& kind, bool& negativeCenter) {
        kind = (choice == 1 || choice == 3) ? core::LaplacianKind::Neighbors8
                                            : core::LaplacianKind::Neighbors4;
        negativeCenter = (choice == 0 || choice == 1);
    }

    std::string SizeText(int size) {
        return std::to_string(size) + "x" + std::to_string(size);
    }

}

core::Kernel AppState::smoothingKernel() const {
    switch (smoothType) {
        case 1:  return core::binomialKernel(smoothSize);
        case 2:  return core::gaussianKernel(smoothSize, static_cast<double>(gaussianSigma));
        default: return core::boxKernel(smoothSize);
    }
}

void AppState::applySmoothing() {
    if (!hasImage) return;
    if (!core::isValidKernelSize(smoothSize)) {
        statusMessage = "Tamanho de filtro inválido: use um valor ímpar.";
        return;
    }

    const core::Kernel kernel = smoothingKernel();
    core::applyToPlanes(workingImage, colorTarget(), [&](core::Image& p) {
        core::applyFilter(p, kernel, core::FilterOperation::Correlation, spatialBorderMode(), true);
    });

    const char* names[] = {"média simples", "média ponderada (1-2-1)", "gaussiano"};
    statusMessage = std::string("Suavização aplicada: ") + names[std::max(0, std::min(smoothType, 2))] +
                    " " + SizeText(smoothSize) + ".";
}

void AppState::applyMedian() {
    if (!hasImage) return;
    if (!core::isValidKernelSize(medianSize)) {
        statusMessage = "Tamanho de filtro inválido: use um valor ímpar.";
        return;
    }
    // Filtro de ordem: por canal pode criar cores falsas; use "so I" para evitar
    core::mapPlanes(workingImage, colorTarget(),
                    [&](core::Image& p) { return core::medianFilter(p, medianSize, spatialBorderMode()); });
    statusMessage = "Filtro da mediana " + SizeText(medianSize) + " aplicado.";
}

void AppState::showLaplacian() {
    if (!hasImage) return;
    core::LaplacianKind kind;
    bool negativeCenter;
    DecodeLaplacian(laplacianChoice, kind, negativeCenter);

    core::mapPlanes(workingImage, core::ColorTarget::ChannelsRGB,
                    [&](core::Image& p) { return core::laplacian(p, kind, negativeCenter, spatialBorderMode()); });
    normalizeNextDisplay = true;
    statusMessage = "Laplaciano calculado (exibido normalizado). Use 'Restaurar imagem original' para voltar.";
}

void AppState::applyLaplacianSharpen() {
    if (!hasImage) return;
    core::LaplacianKind kind;
    bool negativeCenter;
    DecodeLaplacian(laplacianChoice, kind, negativeCenter);

    core::mapPlanes(workingImage, colorTarget(), [&](core::Image& p) {
        return core::sharpenLaplacian(p, kind, negativeCenter, spatialBorderMode());
    });
    statusMessage = std::string("Aguçamento pelo Laplaciano aplicado (c = ") + (negativeCenter ? "-1" : "+1") + ").";
}

void AppState::applyHighBoost() {
    if (!hasImage) return;
    if (!core::isValidKernelSize(smoothSize)) {
        statusMessage = "Tamanho de filtro inválido: use um valor ímpar.";
        return;
    }

    const core::Kernel blur = smoothingKernel();
    core::mapPlanes(workingImage, colorTarget(), [&](core::Image& p) {
        return core::highBoost(p, blur, static_cast<double>(highBoostK), spatialBorderMode());
    });
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.2f", highBoostK);
    statusMessage = std::string(highBoostK > 1.0f ? "High-boost aplicado (k = " : "Máscara de nitidez aplicada (k = ") +
                    buf + ").";
}

void AppState::applySobelX() {
    if (!hasImage) return;
    core::mapPlanes(workingImage, core::ColorTarget::ChannelsRGB,
                    [&](core::Image& p) { return core::sobelX(p, spatialBorderMode()); });
    normalizeNextDisplay = true;
    statusMessage = "Sobel X aplicado (exibido normalizado). Use 'Restaurar imagem original' para voltar.";
}

void AppState::applySobelY() {
    if (!hasImage) return;
    core::mapPlanes(workingImage, core::ColorTarget::ChannelsRGB,
                    [&](core::Image& p) { return core::sobelY(p, spatialBorderMode()); });
    normalizeNextDisplay = true;
    statusMessage = "Sobel Y aplicado (exibido normalizado). Use 'Restaurar imagem original' para voltar.";
}

void AppState::applyGradientMagnitude() {
    if (!hasImage) return;
    workingImage = core::gradientMagnitudeColor(workingImage, spatialBorderMode(), gradientApproximate,
                                                gradientDiZenzo);
    normalizeNextDisplay = true;
    statusMessage = std::string("Magnitude do gradiente calculada (") +
                    (gradientApproximate ? "|gx| + |gy|" : "raiz de gx^2 + gy^2") +
                    (gradientDiZenzo ? ", gradiente vetorial de Di Zenzo" : ", soma dos canais R, G e B") +
                    "; exibida normalizada). Use 'Restaurar imagem original' para voltar.";
}

void AppState::applyHistogramEqualization() {
    if (!hasImage) return;
    // Imagem colorida: equaliza so o plano I do HSI (H e S nao mudam)
    core::equalizeHistogramHSI(workingImage, static_cast<double>(equalizeSaturationGain));
    statusMessage = "Equalização de histograma aplicada ao componente I (HSI).";
}

void AppState::updateHistograms() {
    if (!hasImage || !histogramDirty) return;
    histogramWorking = core::computeColorHistograms(workingImage);
    histogramOriginal = core::computeColorHistograms(originalImage);
    histogramDirty = false;
}

long long AppState::steganographyCapacityBytes() {
    if (!hasImage) return 0;
    return core::steganographyCapacityBytes(workingImage.r());   // a mensagem fica no LSB do canal R
}

bool AppState::embedSteganoMessage() {
    if (!hasImage) return false;

    std::string message(steganoMessageBuffer);
    if (message.empty()) {
        statusMessage = "Digite uma mensagem para esconder antes de aplicar.";
        return false;
    }

    bool ok = core::embedMessageLSB(workingImage.r(), message);
    statusMessage = ok
        ? "Mensagem escondida nos bits menos significativos da imagem."
        : "A mensagem não cabe nesta imagem (reduza o texto ou use uma imagem maior).";
    return ok;
}

void AppState::extractSteganoMessage() {
    if (!hasImage) return;
    steganoExtractedMessage = core::extractMessageLSB(workingImage.r());
    statusMessage = steganoExtractedMessage.empty()
        ? "Nenhuma mensagem escondida foi encontrada nesta imagem."
        : "Mensagem extraída com sucesso.";
}


// ---------------------------------------------------------------------
// Imagens coloridas
// ---------------------------------------------------------------------

void AppState::applyGrayAverage() {
    if (!hasImage) return;
    core::toGrayAverage(workingImage);
    statusMessage = "Escala de cinza aplicada (média aritmética simples: (R+G+B)/3).";
}

void AppState::applyGrayWeighted() {
    if (!hasImage) return;
    core::toGrayWeighted(workingImage, static_cast<double>(grayWeightR), static_cast<double>(grayWeightG),
                         static_cast<double>(grayWeightB));
    statusMessage = "Escala de cinza aplicada (média ponderada).";
}

void AppState::applySepia() {
    if (!hasImage) return;
    core::SepiaParams params;
    params.mode = (sepiaMode == 1) ? core::SepiaMode::HSI : core::SepiaMode::Matrix;
    params.hue = static_cast<double>(sepiaHue);
    params.saturation = static_cast<double>(sepiaSaturation);
    core::sepia(workingImage, params);
    statusMessage = sepiaMode == 1 ? "Sépia aplicada (via HSI)." : "Sépia aplicada (matriz clássica).";
}

bool AppState::loadChromaBackground(std::string& path) {
    std::string pathCopy = path;
    core::ColorImage loaded;
    if (!core::loadImage(pathCopy, loaded)) {
        statusMessage = "Falha ao carregar a imagem de fundo: " + pathCopy;
        return false;
    }
    chromaBackground = loaded;
    hasChromaBackground = true;
    chromaBackgroundName = pathCopy;
    statusMessage = "Imagem de fundo do chroma-key carregada: " + pathCopy;
    return true;
}

core::ChromaKeyParams AppState::chromaKeyParams() const {
    core::ChromaKeyParams p;
    p.keyR = static_cast<int>(std::lround(chromaKeyColor[0] * 255.0f));
    p.keyG = static_cast<int>(std::lround(chromaKeyColor[1] * 255.0f));
    p.keyB = static_cast<int>(std::lround(chromaKeyColor[2] * 255.0f));
    p.tolerance = static_cast<double>(chromaTolerance) / 100.0;
    p.feather = static_cast<double>(chromaFeather) / 100.0;
    p.mode = (chromaMode == 1) ? core::ChromaKeyMode::Cube : core::ChromaKeyMode::Sphere;
    return p;
}

void AppState::applyChromaKey() {
    if (!hasImage) return;
    if (!hasChromaBackground) {
        statusMessage = "Escolha a imagem de fundo antes de aplicar o chroma-key.";
        return;
    }
    core::chromaKey(workingImage, chromaBackground, chromaKeyParams());
    statusMessage = "Chroma-key aplicado (distância = " + std::to_string(static_cast<int>(chromaTolerance)) + "%).";
}

void AppState::pickChromaColor(int row, int col) {
    int r = 0, g = 0, b = 0;
    if (!core::sampleAverageColor(workingImage, row, col, chromaPickRadius, r, g, b)) return;
    chromaKeyColor[0] = static_cast<float>(r) / 255.0f;
    chromaKeyColor[1] = static_cast<float>(g) / 255.0f;
    chromaKeyColor[2] = static_cast<float>(b) / 255.0f;
    chromaPicking = false;
    statusMessage = "Cor-chave escolhida: RGB (" + std::to_string(r) + ", " + std::to_string(g) + ", " +
                    std::to_string(b) + ").";
}

void AppState::applyHsiAdjust() {
    if (!hasImage) return;
    core::adjustHsi(workingImage, static_cast<double>(hsiDeltaHue), static_cast<double>(hsiSaturationFactor),
                    static_cast<double>(hsiIntensityFactor));
    statusMessage = "Ajuste de matiz, saturação e brilho (HSI) aplicado.";
}

void AppState::applyChannelAdjust() {
    if (!hasImage) return;
    core::adjustChannels(workingImage, channelDeltaR, channelDeltaG, channelDeltaB);
    statusMessage = "Ajuste de canal (C/R, M/G, Y/B) aplicado.";
}

void AppState::applyViewToImage() {
    if (!hasImage || viewChoice == 0) return;
    workingImage = core::makeView(workingImage, colorView());
    viewChoice = 0;
    statusMessage = "O plano visualizado agora é a imagem de trabalho (escala de cinza).";
}

}
