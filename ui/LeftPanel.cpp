#include "LeftPanel.hpp"
#include "ImageTexture.hpp"
#include "imgui.h"
#include "tinyfiledialogs.h"
#include <string>

namespace ui {

void RenderLeftPanel(AppState& state, float width, float height) {
    ImGui::BeginChild("PainelMetodos", ImVec2(width, height), true);
    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Métodos e funções");
    ImGui::Separator();

    if (ImGui::Button("Selecionar imagem", ImVec2(-1, 0))) {
        const char* filters[] = { "*.png", "*.jpg", "*.jpeg", "*.bmp", "*.tga" };
        const char* selected = tinyfd_openFileDialog(
            "Selecionar imagem", "", 5, filters, "Imagens", 0);
        if (selected != nullptr) {
            std::string path(selected);
            if (state.loadFromPath(path)) {
                RefreshTexture(state);
            }
        }
    }

    ImGui::BeginDisabled(!state.hasImage);

    if (ImGui::Button("Salvar como PNG", ImVec2(-1, 0))) {
        const char* filters[] = { "*.png" };
        const char* savePath = tinyfd_saveFileDialog(
            "Salvar imagem", "saida.png", 1, filters, "PNG");
        if (savePath != nullptr) {
            std::string path(savePath);
            state.saveToPath(path);
        }
    }

    ImGui::Dummy(ImVec2(0, 10));
    ImGui::Text("Resolução de intensidade (k bits)");
    ImGui::SetNextItemWidth(-1);
    ImGui::SliderInt("##bits", &state.bitsPerPixel, 1, 8, "k = %d bits");
    ImGui::Text("L = 2^k = %lld níveis", 1LL << state.bitsPerPixel);
    if (ImGui::Button("Aplicar quantização", ImVec2(-1, 0))) {
        state.applyQuantization();
        RefreshTexture(state);
    }

    ImGui::Dummy(ImVec2(0, 10));
    ImGui::Text("Resolução espacial (reamostragem)");
    ImGui::SetNextItemWidth(-1);
    ImGui::SliderInt("##resample", &state.resamplePercent, 5, 500, "%d%% do tamanho original");
    ImGui::Text("Interpolação");
    int method = static_cast<int>(state.interpolationMethod);
    ImGui::RadioButton("Vizinho mais próximo", &method, 0);
    ImGui::RadioButton("Bilinear", &method, 1);
    ImGui::RadioButton("Bicúbica", &method, 2);
    state.interpolationMethod = static_cast<core::resampleType>(method);
    
    if (ImGui::Button("Aplicar reamostragem", ImVec2(-1, 0))) {
        state.applyResample();
        RefreshTexture(state);
    }

    ImGui::Dummy(ImVec2(0, 10));
    ImGui::Text("Resolução espacial (DPI)");
    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputFloat("##dpi", &state.dpiValue, 1.0f, 10.0f, "%.1f dpi")) {
        state.applyDpi(state.dpiValue);
    }

    ImGui::Dummy(ImVec2(0, 10));
    if (ImGui::Button("Restaurar imagem original", ImVec2(-1, 0))) {
        state.restoreOriginal();
        RefreshTexture(state);
    }

    ImGui::EndDisabled();
    ImGui::EndChild();
}

}
