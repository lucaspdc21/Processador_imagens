#include "CompareWindow.hpp"
#include "ImageTexture.hpp"
#include "ImageIntensity.hpp"
#include "ImageHistogram.hpp"
#include "ImageStats.hpp"
#include "ImageColor.hpp"
#include <cmath>
#include "imgui.h"
#include <cstdint>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

namespace ui {

void BuildCompareTextures(AppState& state) {
    if (!state.hasImage) return;

    // Ponto de partida comum para todas as transformacoes: a imagem que esta
    // sendo exibida no painel central agora (ja com quantizacao/reamostragem
    // aplicadas, se o usuario tiver usado essas opcoes antes de comparar).
    core::ColorImage& base = state.workingImage;
    const core::ColorTarget target = state.colorTarget();

    std::vector<std::pair<std::string, core::ColorImage>> results;
    results.reserve(16);

    results.emplace_back("Imagem atual", base);

    // Aplica 'op' numa copia da imagem atual e guarda o resultado
    auto add = [&](const std::string& label, core::ColorTarget t, const core::PlaneOp& op) {
        core::ColorImage copy = base;
        core::applyToPlanes(copy, t, op);
        results.emplace_back(label, copy);
    };

    add("1. Negativo", core::ColorTarget::ChannelsRGB, [](core::Image& p) { core::negative(p); });

    char kBuf[32];
    std::snprintf(kBuf, sizeof(kBuf), "%.2f", state.brightnessFactor);
    add(std::string("1b. Brilho (k = ") + kBuf + ")", target,
        [&](core::Image& p) { core::brightness(p, static_cast<double>(state.brightnessFactor)); });

    add("2. Limiarização (m = " + std::to_string(state.thresholdValue) + ")", target,
        [&](core::Image& p) { core::threshold(p, state.thresholdValue); });

    {
        double c = static_cast<double>(state.logConstant);
        if (state.logAutoConstant) {
            int maxVal = core::computeStats(base).maxValue;
            c = (maxVal <= 0) ? 1.0 : 255.0 / std::log(1.0 + static_cast<double>(maxVal));
        }
        add("3. Transformação logarítmica", target, [&](core::Image& p) { core::logTransform(p, c, false); });
    }

    add("4. Correção de gama (γ = " + std::to_string(state.gammaValue) + ")", target, [&](core::Image& p) {
        core::gammaCorrection(p, static_cast<double>(state.gammaValue), static_cast<double>(state.gammaConstant));
    });

    add("5a. Alargamento de contraste", target, [&](core::Image& p) {
        core::contrastStretch(p, state.stretchR1, state.stretchS1, state.stretchR2, state.stretchS2);
    });

    add("5b. Fatiamento de níveis", target, [&](core::Image& p) {
        core::intensitySlice(p, state.sliceMin, state.sliceMax, state.sliceHighlight, state.slicePreserveBackground);
    });

    add("6. Plano de bits " + std::to_string(state.bitPlaneSelected), target,
        [&](core::Image& p) { core::bitPlaneSlice(p, state.bitPlaneSelected); });

    {
        core::ColorImage equalizada = base;
        core::equalizeHistogramHSI(equalizada, static_cast<double>(state.equalizeSaturationGain));
        results.emplace_back("7. Equalização de histograma (HSI)", equalizada);
    }

    // ---- Cor ----
    {
        core::ColorImage cinza = base;
        core::toGrayAverage(cinza);
        results.emplace_back("8a. Cinza: média simples", cinza);
    }
    {
        core::ColorImage cinza = base;
        core::toGrayWeighted(cinza, static_cast<double>(state.grayWeightR), static_cast<double>(state.grayWeightG),
                             static_cast<double>(state.grayWeightB));
        results.emplace_back("8b. Cinza: média ponderada", cinza);
    }
    {
        core::ColorImage sepia = base;
        core::SepiaParams params;
        params.mode = (state.sepiaMode == 1) ? core::SepiaMode::HSI : core::SepiaMode::Matrix;
        params.hue = static_cast<double>(state.sepiaHue);
        params.saturation = static_cast<double>(state.sepiaSaturation);
        core::sepia(sepia, params);
        results.emplace_back("9. Sépia", sepia);
    }
    {
        core::ColorImage ajustada = base;
        core::adjustHsi(ajustada, static_cast<double>(state.hsiDeltaHue), static_cast<double>(state.hsiSaturationFactor),
                        static_cast<double>(state.hsiIntensityFactor));
        results.emplace_back("10. Matiz / saturação / brilho (HSI)", ajustada);
    }
    {
        core::ColorImage ajustada = base;
        core::adjustChannels(ajustada, state.channelDeltaR, state.channelDeltaG, state.channelDeltaB);
        results.emplace_back("11. Ajuste de canal C/R, M/G, Y/B", ajustada);
    }
    if (state.hasChromaBackground) {
        core::ColorImage chave = base;
        core::chromaKey(chave, state.chromaBackground, state.chromaKeyParams());
        results.emplace_back("12. Chroma-key", chave);
    }

    // Garante um slot de textura por resultado, reaproveitando os que ja existem
    // (evita bug)
    if (state.compareTextureIds.size() < results.size()) {
        state.compareTextureIds.resize(results.size(), 0);
    }
    state.compareLabels.clear();

    for (size_t i = 0; i < results.size(); ++i) {
        state.compareLabels.push_back(results[i].first);
        state.compareTextureIds[i] = UploadImageToTexture(results[i].second, state.compareTextureIds[i]);
    }

    state.compareTexturesBuilt = true;
}

void RenderCompareWindow(AppState& state) {
    if (!state.hasImage) {
        state.showCompareWindow = false;
        return;
    }

    if (!state.compareTexturesBuilt) {
        BuildCompareTextures(state);
    }

    ImGui::SetNextWindowSize(ImVec2(1100, 750), ImGuiCond_FirstUseEver);
    bool open = state.showCompareWindow;
    if (ImGui::Begin("Comparação de transformações", &open)) {
        ImGui::TextWrapped(
            "Cada seção mostra o resultado de uma transformação "
            "aplicada a partir da imagem atual do painel central. Ajuste os "
            "parâmetros no painel esquerdo e clique em \"Atualizar comparação\" "
            "para recalcular.");

        if (ImGui::Button("Atualizar comparação")) {
            BuildCompareTextures(state);
        }
        ImGui::Separator();

        const int columns = 3;
        float aspect = static_cast<float>(state.workingImage.getCols()) /
                       static_cast<float>(state.workingImage.getRows());

        if (ImGui::BeginTable("TabelaComparacao", columns,
                               ImGuiTableFlags_Borders | ImGuiTableFlags_SizingStretchSame)) {
            // os rotulos acompanham os resultados atuais (pode haver menos que slots de textura)
            for (size_t i = 0; i < state.compareLabels.size(); ++i) {
                if (i % columns == 0) ImGui::TableNextRow();
                ImGui::TableNextColumn();

                ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "%s", state.compareLabels[i].c_str());

                float drawW = ImGui::GetContentRegionAvail().x;
                float drawH = drawW / aspect;
                ImGui::Image((ImTextureID)(intptr_t)state.compareTextureIds[i], ImVec2(drawW, drawH));
            }
            ImGui::EndTable();
        }
    }
    ImGui::End();

    state.showCompareWindow = open;
}

}
