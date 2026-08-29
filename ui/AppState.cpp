#include "AppState.hpp"
#include "ImageLoad.hpp"
#include "ImageQuantization.hpp"
#include "ImageResample.hpp"
#include <algorithm>

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

    // Parte sempre da original para nao acumular perdas de chamadas anteriores.
    workingImage = originalImage;
    core::quantize(workingImage, bitsPerPixel);
    core::resample(workingImage, newRows, newCols);
    // Amplia de volta ao tamanho original para tornar visivel o efeito de
    // baixa resolucao espacial (pixelizacao), como no experimento da teoria.
    core::resample(workingImage, originalImage.getRows(), originalImage.getCols());

    statusMessage = "Reamostragem aplicada (efeito de baixa resolução espacial).";
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

}
