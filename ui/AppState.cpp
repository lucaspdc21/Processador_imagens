#include "AppState.hpp"
#include "ImageLoad.hpp"
#include "ImageQuantization.hpp"
#include "ImageResample.hpp"
#include "ImageIntensity.hpp"
#include <algorithm>

namespace ui {

namespace {
    // Limites do historico de desfazer (o que estourar primeiro descarta as copias mais antigas).
    constexpr size_t kMaxHistoryEntries = 30;
    constexpr size_t kMaxHistoryBytes = 512ull * 1024 * 1024;
}

bool AppState::loadFromPath(const std::string& path) {
    core::Image loaded;
    if (!core::loadImage(path, loaded)) {
        statusMessage = "Falha ao carregar a imagem: " + path;
        return false;
    }

    originalImage = loaded;
    workingImage = loaded;
    history.clear();
    hasImage = true;
    statsDirty = true;
    bitsPerPixel = workingImage.getBitsPerPixel();
    dpiValue = static_cast<float>(workingImage.getDpi());
    resamplePercent = 100;
    steganoExtractedMessage.clear();
    statusMessage = "Imagem carregada: " + path;
    return true;
}

bool AppState::saveToPath(const std::string& path) {
    if (!hasImage) return false;

    std::string target = core::hasSupportedSaveExtension(path) ? path : path + ".png";
    if (core::saveImage(target, workingImage)) {
        statusMessage = "Imagem salva em: " + target;
        if (core::isJpegPath(target)) {
            statusMessage += " (JPG tem perdas: uma mensagem escondida por esteganografia não sobrevive; use PNG ou BMP)";
        }
        return true;
    }
    statusMessage = "Falha ao salvar a imagem.";
    return false;
}

void AppState::pushHistory() {
    if (!hasImage) return;
    history.push_back(workingImage);

    auto totalBytes = [this]() {
        size_t total = 0;
        for (const core::Image& img : history) total += img.sampleCount() * sizeof(float);
        return total;
    };
    while (history.size() > 1 && (history.size() > kMaxHistoryEntries || totalBytes() > kMaxHistoryBytes)) {
        history.erase(history.begin());
    }
}

void AppState::undo() {
    if (!hasImage || history.empty()) return;
    workingImage = history.back();
    history.pop_back();
    statsDirty = true;
    // Sincroniza os controles com a imagem recuperada.
    bitsPerPixel = workingImage.getBitsPerPixel();
    dpiValue = static_cast<float>(workingImage.getDpi());
    statusMessage = "Última operação desfeita.";
}

void AppState::updateStatsIfNeeded() {
    if (!hasImage || !statsDirty) return;
    cachedStats = core::computeStats(workingImage);
    cachedStorage = core::computeStorageMetrics(workingImage);
    statsDirty = false;
}

void AppState::applyQuantization() {
    if (!hasImage) return;
    pushHistory();
    core::quantize(workingImage, bitsPerPixel);
    statsDirty = true;
    statusMessage = "Quantização aplicada (posterização).";
}

void AppState::applyResample() {
    if (!hasImage) return;

    // Opera sobre a imagem atual (como os demais botoes), sem voltar a original.
    pushHistory();
    int newRows = std::max(1, workingImage.getRows() * resamplePercent / 100);
    int newCols = std::max(1, workingImage.getCols() * resamplePercent / 100);
    core::resample(workingImage, newRows, newCols, interpolationMethod);

    statsDirty = true;
    statusMessage = "Reamostragem aplicada: imagem agora com " + std::to_string(newRows) + " x " +
                    std::to_string(newCols) + " pixels (" + std::to_string(resamplePercent) +
                    "% do tamanho anterior).";
    resamplePercent = 100; // a porcentagem e sempre relativa ao tamanho atual
}

void AppState::applyDpi(float newDpi) {
    if (newDpi < 1.0f) newDpi = 1.0f;
    dpiValue = newDpi;
    workingImage.setDpi(newDpi);
    originalImage.setDpi(newDpi);
}

void AppState::restoreOriginal() {
    if (!hasImage) return;
    pushHistory(); // restaurar tambem pode ser desfeito
    workingImage = originalImage;
    statsDirty = true;
    bitsPerPixel = workingImage.getBitsPerPixel();
    resamplePercent = 100;
    statusMessage = "Imagem restaurada ao estado original.";
}

void AppState::applyBrightness() {
    if (!hasImage) return;
    pushHistory();
    core::brightness(workingImage, brightnessValue);
    statsDirty = true;
    statusMessage = "Brilho aplicado (b = " + std::to_string(brightnessValue) + ").";
}

void AppState::applyNegative() {
    if (!hasImage) return;
    pushHistory();
    core::negative(workingImage);
    statsDirty = true;
    statusMessage = "Negativo da imagem aplicado.";
}

void AppState::applyThreshold() {
    if (!hasImage) return;
    pushHistory();
    core::threshold(workingImage, thresholdValue);
    statsDirty = true;
    statusMessage = "Limiarização aplicada (m = " + std::to_string(thresholdValue) + ").";
}

void AppState::applyLogTransform() {
    if (!hasImage) return;
    pushHistory();
    core::logTransform(workingImage, static_cast<double>(logConstant), logAutoConstant);
    statsDirty = true;
    statusMessage = "Transformação logarítmica aplicada.";
}

void AppState::applyGammaCorrection() {
    if (!hasImage) return;
    pushHistory();
    core::gammaCorrection(workingImage, static_cast<double>(gammaValue), static_cast<double>(gammaConstant));
    statsDirty = true;
    statusMessage = "Correção de gama aplicada (gama = " + std::to_string(gammaValue) + ").";
}

void AppState::applyContrastStretch() {
    if (!hasImage) return;
    pushHistory();
    core::contrastStretch(workingImage, stretchR1, stretchS1, stretchR2, stretchS2);
    statsDirty = true;
    statusMessage = "Alargamento de contraste aplicado.";
}

void AppState::applyIntensitySlice() {
    if (!hasImage) return;
    pushHistory();
    core::intensitySlice(workingImage, sliceMin, sliceMax, sliceHighlight, slicePreserveBackground);
    statsDirty = true;
    statusMessage = "Fatiamento de níveis de intensidade aplicado.";
}

void AppState::applyBitPlaneSlice() {
    if (!hasImage) return;
    pushHistory();
    core::bitPlaneSlice(workingImage, bitPlaneSelected);
    statsDirty = true;
    statusMessage = "Plano de bits " + std::to_string(bitPlaneSelected) + " extraído.";
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

    if (static_cast<long long>(message.size()) > core::steganographyCapacityBytes(workingImage)) {
        statusMessage = "A mensagem não cabe nesta imagem (reduza o texto ou use uma imagem maior).";
        return false;
    }

    pushHistory();
    core::embedMessageLSB(workingImage, message);
    statsDirty = true;
    statusMessage = "Mensagem escondida nos bits menos significativos da imagem. "
                    "Salve em PNG ou BMP e não aplique outras operações antes de salvar.";
    return true;
}

void AppState::extractSteganoMessage() {
    if (!hasImage) return;
    steganoExtractedMessage = core::extractMessageLSB(workingImage);
    statusMessage = steganoExtractedMessage.empty()
        ? "Nenhuma mensagem escondida foi encontrada nesta imagem."
        : "Mensagem extraída com sucesso.";
}

}
