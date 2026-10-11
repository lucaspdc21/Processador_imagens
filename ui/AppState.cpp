#include "AppState.hpp"
#include "ImageLoad.hpp"
#include "ImageQuantization.hpp"
#include "ImageResample.hpp"
#include "ImageIntensity.hpp"
#include <algorithm>
#include <cstdio>

namespace ui {

bool AppState::loadFromPath(std::string& path) {
    std::string pathCopy = path;
    core::Image loaded(0, 0);
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
    core::quantize(workingImage, bitsPerPixel);
    statusMessage = "Quantização aplicada (posterização).";
}

void AppState::applyResample() {
    if (!hasImage) return;

    int newRows = std::max(1, originalImage.getRows() * resamplePercent / 100);
    int newCols = std::max(1, originalImage.getCols() * resamplePercent / 100);

    // Parte sempre da original para nao acumular perdas de chamadas anteriores
    workingImage = originalImage;
    core::quantize(workingImage, bitsPerPixel);
    core::resample(workingImage, newRows, newCols, interpolationMethod);
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
    statusMessage = "Imagem restaurada ao estado original.";
}

void AppState::applyNegative() {
    if (!hasImage) return;
    core::negative(workingImage);
    statusMessage = "Negativo da imagem aplicado.";
}

void AppState::applyBrightness() {
    if (!hasImage) return;
    core::brightness(workingImage, static_cast<double>(brightnessFactor));
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.2f", brightnessFactor);
    statusMessage = std::string("Brilho aplicado (k = ") + buf + ").";
}

void AppState::applyThreshold() {
    if (!hasImage) return;
    core::threshold(workingImage, thresholdValue);
    statusMessage = "Limiarização aplicada (m = " + std::to_string(thresholdValue) + ").";
}

void AppState::applyLogTransform() {
    if (!hasImage) return;
    core::logTransform(workingImage, static_cast<double>(logConstant), logAutoConstant);
    statusMessage = "Transformação logarítmica aplicada.";
}

void AppState::applyGammaCorrection() {
    if (!hasImage) return;
    core::gammaCorrection(workingImage, static_cast<double>(gammaValue), static_cast<double>(gammaConstant));
    statusMessage = "Correção de gama aplicada (gama = " + std::to_string(gammaValue) + ").";
}

void AppState::applyContrastStretch() {
    if (!hasImage) return;
    core::contrastStretch(workingImage, stretchR1, stretchS1, stretchR2, stretchS2);
    statusMessage = "Alargamento de contraste aplicado.";
}

void AppState::applyIntensitySlice() {
    if (!hasImage) return;
    core::intensitySlice(workingImage, sliceMin, sliceMax, sliceHighlight, slicePreserveBackground);
    statusMessage = "Fatiamento de níveis de intensidade aplicado.";
}

void AppState::applyBitPlaneSlice() {
    if (!hasImage) return;
    core::bitPlaneSlice(workingImage, bitPlaneSelected);
    statusMessage = "Plano de bits " + std::to_string(bitPlaneSelected) + " extraído.";
}

void AppState::applyHistogramEqualization() {
    if (!hasImage) return;
    core::equalizeHistogram(workingImage);
    statusMessage = "Equalização de histograma aplicada.";
}

void AppState::updateHistograms() {
    if (!hasImage || !histogramDirty) return;
    histogramWorking = core::computeHistogram(workingImage);
    histogramOriginal = core::computeHistogram(originalImage);
    histogramDirty = false;
}

long long AppState::steganographyCapacityBytes() {
    if (!hasImage) return 0;
    return core::steganographyCapacityBytes(workingImage);
}

bool AppState::embedSteganoMessage() {
    if (!hasImage) return false;

    std::string message(steganoMessageBuffer);
    if (message.empty()) {
        statusMessage = "Digite uma mensagem para esconder antes de aplicar.";
        return false;
    }

    bool ok = core::embedMessageLSB(workingImage, message);
    statusMessage = ok
        ? "Mensagem escondida nos bits menos significativos da imagem."
        : "A mensagem não cabe nesta imagem (reduza o texto ou use uma imagem maior).";
    return ok;
}

void AppState::extractSteganoMessage() {
    if (!hasImage) return;
    steganoExtractedMessage = core::extractMessageLSB(workingImage);
    statusMessage = steganoExtractedMessage.empty()
        ? "Nenhuma mensagem escondida foi encontrada nesta imagem."
        : "Mensagem extraída com sucesso.";
}

}
