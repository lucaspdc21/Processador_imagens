#include "RightPanel.hpp"
#include "ImageQuantization.hpp"
#include "ImageStats.hpp"
#include "ImageMetrics.hpp"
#include "imgui.h"
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <vector>

namespace ui {

namespace {

    // Desenha o histograma como grafico de barras: h(rk) = nk versus rk, ou
    // p(rk) = nk/MN versus rk quando normalizado (Cap. 3.3).
    //
    // Desenho proprio em vez de ImGui::PlotHistogram: com o painel mais estreito que
    // 256 px o PlotHistogram PULA niveis (desenha 1 a cada N) e picos de um unico nivel
    // podem sumir. Aqui cada coluna de pixel usa o MAIOR valor dos niveis que caem nela.
    // 'logScale' (eixo y em log(1 + n)) so muda o desenho, nao os dados: evita que os
    // picos em 0 e 255 (pixels saturados) achatem o resto do grafico.
    void DrawHistogram(const char* id, const core::Histogram& h, bool normalized, bool logScale, float plotHeight,
                       const char* overlayText = nullptr) {
        const ImGuiStyle& style = ImGui::GetStyle();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const float width = std::max(32.0f, ImGui::GetContentRegionAvail().x);
        const ImVec2 p0 = ImGui::GetCursorScreenPos();
        const ImVec2 p1(p0.x + width, p0.y + plotHeight);
        ImGui::InvisibleButton(id, ImVec2(width, plotHeight));
        const bool hovered = ImGui::IsItemHovered();

        dl->AddRectFilled(p0, p1, ImGui::GetColorU32(ImGuiCol_FrameBg), style.FrameRounding);
        const ImVec2 i0(p0.x + style.FramePadding.x, p0.y + style.FramePadding.y);
        const ImVec2 i1(p1.x - style.FramePadding.x, p1.y - style.FramePadding.y);

        const int L = h.levels();
        long long peak = 0;
        for (long long n : h.counts) peak = std::max(peak, n);

        if (L > 0 && h.totalPixels > 0 && peak > 0 && i1.x > i0.x && i1.y > i0.y) {
            const ImU32 colBar = ImGui::GetColorU32(ImGuiCol_PlotHistogram);
            const ImU32 colHover = ImGui::GetColorU32(ImGuiCol_PlotHistogramHovered);
            const int buckets = std::max(1, std::min(L, static_cast<int>(i1.x - i0.x)));
            const float bw = (i1.x - i0.x) / static_cast<float>(buckets);
            auto scaled = [&](long long n) { return logScale ? std::log1p(static_cast<double>(n)) : static_cast<double>(n); };
            const double top = scaled(peak);

            int hoveredBucket = -1;
            if (hovered) {
                hoveredBucket = static_cast<int>((ImGui::GetIO().MousePos.x - i0.x) / bw);
                hoveredBucket = std::max(0, std::min(buckets - 1, hoveredBucket));
            }
            int hoverLevel = -1;
            long long hoverCount = 0;

            for (int b = 0; b < buckets; ++b) {
                const int a = static_cast<int>(static_cast<long long>(b) * L / buckets);
                const int e = std::max(a + 1, static_cast<int>(static_cast<long long>(b + 1) * L / buckets));
                long long n = 0;
                int level = a;
                for (int k = a; k < e && k < L; ++k) {
                    if (h.counts[static_cast<size_t>(k)] > n) { n = h.counts[static_cast<size_t>(k)]; level = k; }
                }
                if (b == hoveredBucket) { hoverLevel = level; hoverCount = n; }
                if (n <= 0) continue;
                const float bar = static_cast<float>(scaled(n) / top) * (i1.y - i0.y);
                dl->AddRectFilled(ImVec2(i0.x + static_cast<float>(b) * bw, i1.y - bar),
                                  ImVec2(i0.x + static_cast<float>(b + 1) * bw, i1.y),
                                  b == hoveredBucket ? colHover : colBar);
            }

            if (hoverLevel >= 0) {
                const double p = static_cast<double>(hoverCount) / static_cast<double>(h.totalPixels);
                if (normalized) ImGui::SetTooltip("r = %d  |  p = %.5f  (n = %lld)", hoverLevel, p, hoverCount);
                else            ImGui::SetTooltip("r = %d  |  n = %lld  (p = %.5f)", hoverLevel, hoverCount, p);
            }
        }

        if (overlayText) {
            const ImVec2 ts = ImGui::CalcTextSize(overlayText);
            dl->AddText(ImVec2(p0.x + (width - ts.x) * 0.5f, p0.y + style.FramePadding.y),
                        ImGui::GetColorU32(ImGuiCol_Text), overlayText);
        }
    }


    // Quatro histogramas (R, G, B e I), cada um na sua cor. Se a imagem e cinza
    // (R = G = B) eles sao identicos, entao desenha um so (cinza = R = G = B = I).
    void DrawColorHistograms(const char* title, const char* idSuffix, const core::ColorHistograms& h, bool gray,
                             bool normalized, bool logScale, float plotHeight) {
        struct Entry { const char* name; const core::Histogram* hist; ImVec4 color; };
        const Entry colorEntries[] = {
            {"R", &h.r, ImVec4(0.90f, 0.25f, 0.25f, 1.0f)},
            {"G", &h.g, ImVec4(0.25f, 0.80f, 0.30f, 1.0f)},
            {"B", &h.b, ImVec4(0.30f, 0.50f, 0.95f, 1.0f)},
            {"I", &h.i, ImVec4(0.75f, 0.75f, 0.75f, 1.0f)},
        };
        const Entry grayEntries[] = {
            {"Cinza", &h.i, ImVec4(0.75f, 0.75f, 0.75f, 1.0f)},
        };
        const Entry* entries = gray ? grayEntries : colorEntries;
        const int count = gray ? 1 : 4;

        ImGui::Text("%s", title);
        if (gray) {
            ImGui::SameLine();
            ImGui::TextDisabled("(cinza: R=G=B)");
        }
        for (int k = 0; k < count; ++k) {
            const Entry& e = entries[k];
            char id[48];
            std::snprintf(id, sizeof(id), "##hist%s_%s", e.name, idSuffix);
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, e.color);
            DrawHistogram(id, *e.hist, normalized, logScale, gray ? plotHeight * 1.5f : plotHeight, e.name);
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
        ImGui::Checkbox("Escala logarítmica (eixo y)", &state.histogramLogScale);

        DrawColorHistograms("Imagem atual", "atual", state.histogramWorking, state.histogramWorkingGray,
                            state.histogramNormalized, state.histogramLogScale, 70.0f);
        ImGui::TextDisabled("0 (preto) ... 255 (branco)");
        core::HistogramStats hs = core::computeHistogramStats(state.histogramWorking.i);
        ImGui::TextWrapped("%s: média %.2f | desvio %.2f | mediana %d (%.2f)", state.histogramWorkingGray ? "Cinza" : "I",
                    hs.mean, hs.stdDev, hs.median, hs.median / 255.0);

        ImGui::Dummy(ImVec2(0, 4));
        DrawColorHistograms("Imagem original", "original", state.histogramOriginal, state.histogramOriginalGray,
                            state.histogramNormalized, state.histogramLogScale, 50.0f);
        core::HistogramStats ho = core::computeHistogramStats(state.histogramOriginal.i);
        ImGui::TextWrapped("%s: média %.2f | desvio %.2f | mediana %d (%.2f)", state.histogramOriginalGray ? "Cinza" : "I",
                    ho.mean, ho.stdDev, ho.median, ho.median / 255.0);
    } else {
        ImGui::TextWrapped("Carregue uma imagem para ver suas informações.");
    }

    ImGui::EndChild();
}

}
