#include "RightPanel.hpp"
#include "ImageQuantization.hpp"
#include "ImageStats.hpp"
#include "ImageMetrics.hpp"
#include "imgui.h"
#include <cfloat>
#include <vector>

namespace ui {

namespace {

    // Desenha o histograma como grafico de barras: h(rk) = nk versus rk, ou
    // p(rk) = nk/MN versus rk quando normalizado (Cap. 3.3).
    void DrawHistogram(const char* id, const core::Histogram& h, bool normalized, float plotHeight) {
        std::vector<float> values(h.counts.size());
        for (size_t i = 0; i < values.size(); ++i) {
            double v = static_cast<double>(h.counts[i]);
            if (normalized && h.totalPixels > 0) v /= static_cast<double>(h.totalPixels);
            values[i] = static_cast<float>(v);
        }
        ImGui::PlotHistogram(id, values.data(), static_cast<int>(values.size()),
                             0, nullptr, 0.0f, FLT_MAX, ImVec2(-1, plotHeight));
    }

}

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

        // Histograma da imagem atual e da original
        state.updateHistograms();
        ImGui::SeparatorText("Histograma");
        ImGui::Checkbox("Normalizado: p(rk) = nk/MN", &state.histogramNormalized);

        ImGui::Text("Imagem atual");
        DrawHistogram("##histAtual", state.histogramWorking, state.histogramNormalized, 110.0f);
        ImGui::TextDisabled("0 (preto) ... 255 (branco)");
        core::HistogramStats hs = core::computeHistogramStats(state.histogramWorking);
        ImGui::Text("Média: %.2f | Desvio padrão: %.2f", hs.mean, hs.stdDev);

        ImGui::Dummy(ImVec2(0, 4));
        ImGui::Text("Imagem original");
        DrawHistogram("##histOriginal", state.histogramOriginal, state.histogramNormalized, 70.0f);
    } else {
        ImGui::TextWrapped("Carregue uma imagem para ver suas informações.");
    }

    ImGui::EndChild();
}

}
