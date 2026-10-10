#include "CompareWindow.hpp"
#include "ImageTexture.hpp"
#include "ImageIntensity.hpp"
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
    core::Image& base = state.workingImage;

    std::vector<std::pair<std::string, core::Image>> results;
    results.reserve(9);

    results.emplace_back("Imagem atual", base);

    core::Image negativa = base;
    core::negative(negativa);
    results.emplace_back("1. Negativo", negativa);

    core::Image brilho = base;
    core::brightness(brilho, static_cast<double>(state.brightnessFactor));
    char kBuf[32];
    std::snprintf(kBuf, sizeof(kBuf), "%.2f", state.brightnessFactor);
    results.emplace_back(std::string("1b. Brilho (k = ") + kBuf + ")", brilho);

    core::Image limiarizada = base;
    core::threshold(limiarizada, state.thresholdValue);
    results.emplace_back("2. Limiarização (m = " + std::to_string(state.thresholdValue) + ")", limiarizada);

    core::Image logaritmica = base;
    core::logTransform(logaritmica, static_cast<double>(state.logConstant), state.logAutoConstant);
    results.emplace_back("3. Transformação logarítmica", logaritmica);

    core::Image potencia = base;
    core::gammaCorrection(potencia, static_cast<double>(state.gammaValue), static_cast<double>(state.gammaConstant));
    results.emplace_back("4. Correção de gama (γ = " + std::to_string(state.gammaValue) + ")", potencia);

    core::Image alargada = base;
    core::contrastStretch(alargada, state.stretchR1, state.stretchS1, state.stretchR2, state.stretchS2);
    results.emplace_back("5a. Alargamento de contraste", alargada);

    core::Image fatiada = base;
    core::intensitySlice(fatiada, state.sliceMin, state.sliceMax, state.sliceHighlight, state.slicePreserveBackground);
    results.emplace_back("5b. Fatiamento de níveis", fatiada);

    core::Image plano = base;
    core::bitPlaneSlice(plano, state.bitPlaneSelected);
    results.emplace_back("6. Plano de bits " + std::to_string(state.bitPlaneSelected), plano);

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
    if (ImGui::Begin("Comparação de transformações de intensidade", &open)) {
        ImGui::TextWrapped(
            "Cada seção mostra o resultado de uma transformação de intensidade "
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
            for (size_t i = 0; i < state.compareTextureIds.size(); ++i) {
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
