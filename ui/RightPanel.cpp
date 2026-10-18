#include "RightPanel.hpp"
#include "ImageQuantization.hpp"
#include "ImageStats.hpp"
#include "ImageMetrics.hpp"
#include "imgui.h"
#include <cfloat>
#include <cstdio>
#include <vector>

namespace ui {

namespace {

    // Desenha o histograma como grafico de barras: h(rk) = nk versus rk, ou
    // p(rk) = nk/MN versus rk quando normalizado (Cap. 3.3).
    void DrawHistogram(const char* id, const core::Histogram& h, bool normalized, float plotHeight,
                       const char* overlayText = nullptr) {
        std::vector<float> values(h.counts.size());
        for (size_t i = 0; i < values.size(); ++i) {
            double v = static_cast<double>(h.counts[i]);
            if (normalized && h.totalPixels > 0) v /= static_cast<double>(h.totalPixels);
            values[i] = static_cast<float>(v);
        }
        ImGui::PlotHistogram(id, values.data(), static_cast<int>(values.size()),
                             0, overlayText, 0.0f, FLT_MAX, ImVec2(-1, plotHeight));
    }


    // Quatro histogramas (R, G, B e I), cada um na sua cor
    void DrawColorHistograms(const char* title, const char* idSuffix, const core::ColorHistograms& h,
                             bool normalized, float plotHeight) {
        struct Entry { const char* name; const core::Histogram* hist; ImVec4 color; };
        const Entry entries[] = {
            {"R", &h.r, ImVec4(0.90f, 0.25f, 0.25f, 1.0f)},
            {"G", &h.g, ImVec4(0.25f, 0.80f, 0.30f, 1.0f)},
            {"B", &h.b, ImVec4(0.30f, 0.50f, 0.95f, 1.0f)},
            {"I", &h.i, ImVec4(0.75f, 0.75f, 0.75f, 1.0f)},
        };
        ImGui::Text("%s", title);
        for (const Entry& e : entries) {
            char id[48];
            std::snprintf(id, sizeof(id), "##hist%s_%s", e.name, idSuffix);
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, e.color);
            DrawHistogram(id, *e.hist, normalized, plotHeight, e.name);
            ImGui::PopStyleColor();
        }
    }

}

void RenderRightPanel(AppState& state, float width, float height) {
    ImGui::BeginChild("PainelInfo", ImVec2(width, height), true);
    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Informações da imagem");
    ImGui::Separator();

    if (state.hasImage) {
        core::ColorImage& img = state.workingImage;
        core::ImageStats stats = core::computeStats(img);
        core::SpatialMetrics spatial = core::computeSpatialMetrics(img);
        core::StorageMetrics storage = core::computeStorageMetrics(img);

        ImGui::Text("Tamanho (M x N): %d x %d", img.getRows(), img.getCols());
        ImGui::Text("Total de pixels: %lld",
                    static_cast<long long>(img.getRows()) * img.getCols());

        ImGui::Dummy(ImVec2(0, 8));
        ImGui::Text("Intensidade luminosa");
        ImGui::Text("k (bits/canal): %d  (3 canais: R, G, B)", img.getBitsPerPixel());
        ImGui::Text("L = 2^k: %lld níveis por canal", core::levelsForBits(img.getBitsPerPixel()));

        ImGui::Dummy(ImVec2(0, 8));
        ImGui::Text("Resolução espacial");
        ImGui::Text("DPI: %.1f", img.getDpi());
        ImGui::Text("Tamanho físico: %.2f x %.2f pol.",
                    spatial.widthInches, spatial.heightInches);

        ImGui::Dummy(ImVec2(0, 8));

        // Histogramas R, G, B e I da imagem atual e da original
        state.updateHistograms();
        ImGui::SeparatorText("Histogramas");
        ImGui::Checkbox("Normalizado: p(rk) = nk/MN", &state.histogramNormalized);

        DrawColorHistograms("Imagem atual", "atual", state.histogramWorking, state.histogramNormalized, 70.0f);
        ImGui::TextDisabled("0 (preto) ... 255 (branco)");
        core::HistogramStats hs = core::computeHistogramStats(state.histogramWorking.i);
        ImGui::Text("I: média %.2f | desvio %.2f | mediana %d (%.2f)", hs.mean, hs.stdDev, hs.median,
                    hs.median / 255.0);

        ImGui::Dummy(ImVec2(0, 4));
        DrawColorHistograms("Imagem original", "original", state.histogramOriginal, state.histogramNormalized, 50.0f);
        core::HistogramStats ho = core::computeHistogramStats(state.histogramOriginal.i);
        ImGui::Text("I: média %.2f | desvio %.2f | mediana %d (%.2f)", ho.mean, ho.stdDev, ho.median,
                    ho.median / 255.0);
    } else {
        ImGui::TextWrapped("Carregue uma imagem para ver suas informações.");
    }

    ImGui::EndChild();
}

}
