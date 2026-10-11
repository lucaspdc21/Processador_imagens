#include "RightPanel.hpp"
#include "ImageQuantization.hpp"
#include "ImageMetrics.hpp"
#include "imgui.h"

namespace ui {

void RenderRightPanel(AppState& state, float width, float height) {
    ImGui::BeginChild("PainelInfo", ImVec2(width, height), true);
    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Informações da imagem");
    ImGui::Separator();

    if (state.hasImage) {
        const core::Image& img = state.workingImage;
        // Estatisticas e armazenamento so sao recalculados quando a imagem muda.
        state.updateStatsIfNeeded();
        const core::ImageStats& stats = state.cachedStats;
        const core::StorageMetrics& storage = state.cachedStorage;
        core::SpatialMetrics spatial = core::computeSpatialMetrics(img);

        ImGui::Text("Tamanho (M x N): %d x %d", img.getRows(), img.getCols());
        ImGui::Text("Total de pixels: %lld",
                    static_cast<long long>(img.getRows()) * img.getCols());
        ImGui::Text("Canais: %d (%s)", img.getChannels(), img.getChannels() == 3 ? "RGB" : "cinza");

        ImGui::Dummy(ImVec2(0, 8));
        ImGui::Text("Intensidade luminosa");
        ImGui::Text("k (bits/pixel): %d", img.getBitsPerPixel());
        ImGui::Text("L = 2^k: %lld níveis", core::levelsForBits(img.getBitsPerPixel()));
        ImGui::Text("Mínimo: %d | Máximo: %d", stats.minValue, stats.maxValue);
        ImGui::Text("Faixa dinâmica: %d", stats.dynamicRange);
        ImGui::Text("Contraste (Michelson): %.3f", stats.contrast);

        ImGui::Dummy(ImVec2(0, 8));
        ImGui::Text("Resolução espacial");
        ImGui::Text("DPI: %.1f", img.getDpi());
        ImGui::Text("Tamanho físico: %.2f x %.2f pol.",
                    spatial.widthInches, spatial.heightInches);

        ImGui::Dummy(ImVec2(0, 8));
        ImGui::Text("Armazenamento");
        ImGui::Text("Bits: %lld", storage.bits);
        ImGui::Text("Bytes: %.0f", storage.bytes);
        ImGui::Text("%.2f KB | %.3f MB", storage.kilobytes, storage.megabytes);

        ImGui::Dummy(ImVec2(0, 8));
    } else {
        ImGui::TextWrapped("Carregue uma imagem para ver suas informações.");
    }

    ImGui::EndChild();
}

}
