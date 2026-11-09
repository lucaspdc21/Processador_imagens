#include "TransformCurve.hpp"
#include "imgui.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace ui {

namespace {
    constexpr float kMargin = 4.0f;   // folga para nada ser cortado nas bordas
}

void TransformCurvePlot(const char* id, const CurveLut& lut, unsigned int curveColor,
                        const core::Histogram* backdrop, float side) {
    ImGui::PushID(id);
    ImDrawList* dl = ImGui::GetWindowDrawList();

    if (side <= 0.0f) side = ImGui::GetContentRegionAvail().x;
    side = std::max(120.0f, std::min(side, 320.0f));

    const ImVec2 origin = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("##canvas", ImVec2(side, side));
    const bool hovered = ImGui::IsItemHovered();

    const float x0 = origin.x + kMargin, x1 = origin.x + side - kMargin;
    const float y0 = origin.y + side - kMargin, y1 = origin.y + kMargin;   // y0: s = 0 (embaixo), y1: s = 255 (em cima)
    auto toX = [&](float r) { return x0 + (x1 - x0) * r / 255.0f; };
    auto toY = [&](float s) { return y0 + (y1 - y0) * s / 255.0f; };
    auto toR = [&](float px) { return (px - x0) / (x1 - x0) * 255.0f; };

    const ImU32 colBg = ImGui::GetColorU32(ImGuiCol_FrameBg);
    const ImU32 colGrid = ImGui::GetColorU32(ImGuiCol_Border, 0.55f);
    const ImU32 colDiag = ImGui::GetColorU32(ImGuiCol_TextDisabled, 0.8f);
    const ImU32 colText = ImGui::GetColorU32(ImGuiCol_TextDisabled);
    const ImU32 colBackdrop = IM_COL32(150, 150, 150, 80);
    const ImU32 colSat = IM_COL32(255, 80, 80, 34);
    const ImU32 colCrush = IM_COL32(80, 140, 255, 34);
    const ImU32 colGuide = IM_COL32(255, 255, 255, 140);

    // Fundo e grade (a cada 64 niveis, como no editor de contraste)
    dl->AddRectFilled(origin, ImVec2(origin.x + side, origin.y + side), colBg);
    for (int v = 0; v <= 256; v += 64) {
        const float f = static_cast<float>(std::min(v, 255));
        dl->AddLine(ImVec2(toX(f), y0), ImVec2(toX(f), y1), colGrid);
        dl->AddLine(ImVec2(x0, toY(f)), ImVec2(x1, toY(f)), colGrid);
    }

    // Histograma ao fundo (log(1 + n), ocupa no maximo 30% da altura). Cada coluna
    // de pixel resume os niveis que caem nela pelo maior valor, para nenhum pico
    // sumir quando o grafico tem menos de 256 px de largura.
    const bool hasBackdrop = backdrop != nullptr && backdrop->levels() == 256 && backdrop->totalPixels > 0;
    if (hasBackdrop) {
        long long peak = 0;
        for (long long n : backdrop->counts) peak = std::max(peak, n);
        if (peak > 0) {
            const double logPeak = std::log1p(static_cast<double>(peak));
            const float maxH = 0.30f * (y0 - y1);
            const int cols = std::max(1, static_cast<int>(x1 - x0));
            for (int c = 0; c < cols; ++c) {
                const int a = static_cast<int>(static_cast<long long>(c) * 256 / cols);
                const int b = std::max(a + 1, static_cast<int>(static_cast<long long>(c + 1) * 256 / cols));
                long long n = 0;
                for (int k = a; k < b && k < 256; ++k) n = std::max(n, backdrop->counts[static_cast<size_t>(k)]);
                if (n <= 0) continue;
                const float h = static_cast<float>(std::log1p(static_cast<double>(n)) / logPeak) * maxH;
                dl->AddRectFilled(ImVec2(x0 + static_cast<float>(c), y0 - h), ImVec2(x0 + static_cast<float>(c + 1), y0), colBackdrop);
            }
        }
    }

    // Faixas de entrada que saturam em 255 (r >= satFrom) ou vao a 0 (1 <= r <= crushTo)
    int satFrom = -1;
    for (int r = 0; r < 255; ++r) {
        if (lut[static_cast<size_t>(r)] >= 255) { satFrom = r; break; }
    }
    int crushTo = 0;
    while (crushTo + 1 < 255 && lut[static_cast<size_t>(crushTo + 1)] <= 0) ++crushTo;
    // So vale a pena avisar quando a faixa tem pelo menos 3 niveis: faixas menores
    // sao apenas arredondamento (ex.: 254,6 -> 255), nao perda real de informacao.
    if (satFrom > 252) satFrom = -1;
    if (crushTo < 3) crushTo = 0;
    if (satFrom >= 0) dl->AddRectFilled(ImVec2(toX(static_cast<float>(satFrom)), y1), ImVec2(x1, y0), colSat);
    if (crushTo >= 1) dl->AddRectFilled(ImVec2(x0, y1), ImVec2(toX(static_cast<float>(crushTo)), y0), colCrush);

    // Reta identidade s = r (referencia: sem alteracao) e a curva T(r)
    dl->AddLine(ImVec2(x0, y0), ImVec2(x1, y1), colDiag, 1.0f);
    ImVec2 pts[256];
    for (int r = 0; r < 256; ++r) {
        const float s = static_cast<float>(std::max(0, std::min(255, lut[static_cast<size_t>(r)])));
        pts[r] = ImVec2(toX(static_cast<float>(r)), toY(s));
    }
    dl->AddPolyline(pts, 256, curveColor, ImDrawFlags_None, 2.0f);

    // Rotulos dos eixos
    dl->AddText(ImVec2(origin.x + 6.0f, origin.y + 4.0f), colText, "s");
    dl->AddText(ImVec2(origin.x + side - 14.0f, origin.y + side - 18.0f), colText, "r");

    // Interacao: o mouse sobre o grafico le T(r) em qualquer nivel
    if (hovered) {
        const ImVec2 m = ImGui::GetIO().MousePos;
        const int r = std::max(0, std::min(255, static_cast<int>(std::lround(toR(m.x)))));
        const int s = std::max(0, std::min(255, lut[static_cast<size_t>(r)]));
        const ImVec2 pt(toX(static_cast<float>(r)), toY(static_cast<float>(s)));
        dl->AddLine(ImVec2(pt.x, y0), pt, colGuide);
        dl->AddLine(ImVec2(x0, pt.y), pt, colGuide);
        dl->AddCircleFilled(pt, 4.5f, IM_COL32(255, 255, 255, 255));
        dl->AddCircle(pt, 4.5f, IM_COL32(0, 0, 0, 200), 0, 1.5f);

        ImGui::BeginTooltip();
        ImGui::Text("r = %d  ->  s = %d  (%+d)", r, s, s - r);
        if (hasBackdrop) {
            const long long n = backdrop->counts[static_cast<size_t>(r)];
            ImGui::TextDisabled("pixels com I = %d: %lld (%.2f%%)", r, n,
                                100.0 * static_cast<double>(n) / static_cast<double>(backdrop->totalPixels));
        }
        ImGui::EndTooltip();
    }

    ImGui::SetCursorScreenPos(ImVec2(origin.x, origin.y + side));
    ImGui::Dummy(ImVec2(0.0f, 2.0f));

    // Legenda das faixas coloridas (so aparece quando existem)
    if (satFrom >= 0) ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.45f, 1.0f), "Satura em 255 para r >= %d", satFrom);
    if (crushTo >= 1) ImGui::TextColored(ImVec4(0.45f, 0.65f, 1.0f, 1.0f), "Vai a 0 para r <= %d", crushTo);

    ImGui::PopID();
}

}
