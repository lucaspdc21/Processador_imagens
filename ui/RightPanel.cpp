#include "RightPanel.hpp"
#include "ImageQuantization.hpp"
#include "ImageStats.hpp"
#include "ImageMetrics.hpp"
#include "imgui.h"

namespace ui {

void RenderRightPanel(AppState& state, float width, float height) {
    ImGui::BeginChild("PainelInfo", ImVec2(width, height), true);
    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Informações da imagem");
    ImGui::Separator();

    if (state.hasImage) {
        core::Image& img = state.workingImage;
        core::ImageStats stats = core::computeStats(img);
        core::SpatialMetrics spatial = core::computeSpatialMetrics(img);
        core::StorageMetrics storage = core::computeStorageMetrics(img);

        ImGui::Text("Tamanho (M x N): %d x %d", img.getRows(), img.getCols());
        ImGui::Text("Total de pixels: %lld",
                    static_cast<long long>(img.getRows()) * img.getCols());

        ImGui::Dummy(ImVec2(0, 8));
        ImGui::Text("Intensidade luminosa");
        ImGui::Text("k (bits/pixel): %d", img.getBitsPerPixel());
        ImGui::Text("L = 2^k: %lld níveis", core::levelsForBits(img.getBitsPerPixel()));

        ImGui::Dummy(ImVec2(0, 8));
        ImGui::Text("Resolução espacial");
        ImGui::Text("DPI: %.1f", img.getDpi());
        ImGui::Text("Tamanho físico: %.2f x %.2f pol.",
                    spatial.widthInches, spatial.heightInches);

        ImGui::Dummy(ImVec2(0, 8));
    } else {
        ImGui::TextWrapped("Carregue uma imagem para ver suas informações.");
    }

    ImGui::EndChild();
}

}
