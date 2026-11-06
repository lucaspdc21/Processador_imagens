#include "ContrastCurve.hpp"
#include "ImageIntensity.hpp"
#include "imgui.h"
#include <algorithm>
#include <cmath>

namespace ui {

namespace {

    constexpr float kHandleRadius = 6.0f;   // raio visual do ponto
    constexpr float kHitRadius = 10.0f;     // raio da area clicavel
    constexpr float kMargin = 4.0f;         // folga para os pontos nas bordas nao serem cortados

    int clampLevel(float v) {
        return std::max(0, std::min(255, static_cast<int>(std::lround(v))));
    }

}

bool ContrastCurveEditor(const char* id, int& r1, int& s1, int& r2, int& s2, float side) {
    ImGui::PushID(id);
    ImDrawList* dl = ImGui::GetWindowDrawList();

    if (side <= 0.0f) side = ImGui::GetContentRegionAvail().x;
    side = std::max(side, 120.0f);

    // Area do grafico (quadrada). O botao invisivel reserva o espaco e permite
    // que os botoes dos pontos, enviados depois, fiquem por cima dele.
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    ImGui::SetNextItemAllowOverlap();
    ImGui::InvisibleButton("##canvas", ImVec2(side, side));
    const float x0 = origin.x + kMargin, x1 = origin.x + side - kMargin;
    const float y0 = origin.y + side - kMargin, y1 = origin.y + kMargin;   // y0: s = 0 (embaixo), y1: s = 255 (em cima)
    auto toX = [&](float r) { return x0 + (x1 - x0) * r / 255.0f; };
    auto toY = [&](float s) { return y0 + (y1 - y0) * s / 255.0f; };
    auto toR = [&](float px) { return (px - x0) / (x1 - x0) * 255.0f; };
    auto toS = [&](float py) { return (py - y0) / (y1 - y0) * 255.0f; };

    const ImU32 colBg = ImGui::GetColorU32(ImGuiCol_FrameBg);
    const ImU32 colGrid = ImGui::GetColorU32(ImGuiCol_Border, 0.55f);
    const ImU32 colDiag = ImGui::GetColorU32(ImGuiCol_TextDisabled, 0.8f);
    const ImU32 colCurve = IM_COL32(255, 200, 70, 255);
    const ImU32 colPoint = IM_COL32(90, 170, 255, 255);
    const ImU32 colPointActive = IM_COL32(255, 255, 255, 255);
    const ImU32 colFixed = ImGui::GetColorU32(ImGuiCol_TextDisabled);

    // Fundo e grade (a cada 64 niveis)
    dl->AddRectFilled(origin, ImVec2(origin.x + side, origin.y + side), colBg);
    for (int v = 0; v <= 256; v += 64) {
        const float f = static_cast<float>(std::min(v, 255));
        dl->AddLine(ImVec2(toX(f), y0), ImVec2(toX(f), y1), colGrid);
        dl->AddLine(ImVec2(x0, toY(f)), ImVec2(x1, toY(f)), colGrid);
    }
    // Reta identidade s = r (referencia: sem alteracao)
    dl->AddLine(ImVec2(x0, y0), ImVec2(x1, y1), colDiag, 1.0f);

    bool changed = false;
    bool pointActive[2] = {false, false};

    // Pontos arrastaveis. A curva usa os valores ja atualizados neste quadro.
    struct Handle { const char* name; int* r; int* s; int which; };
    Handle handles[2] = {{"##p1", &r1, &s1, 1}, {"##p2", &r2, &s2, 2}};
    for (const Handle& h : handles) {
        ImGui::SetCursorScreenPos(ImVec2(toX(static_cast<float>(*h.r)) - kHitRadius,
                                         toY(static_cast<float>(*h.s)) - kHitRadius));
        ImGui::InvisibleButton(h.name, ImVec2(2.0f * kHitRadius, 2.0f * kHitRadius));
        pointActive[h.which - 1] = ImGui::IsItemActive();
        if (ImGui::IsItemActive() && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            const ImVec2 m = ImGui::GetIO().MousePos;
            int nr = clampLevel(toR(m.x));
            const int ns = clampLevel(toS(m.y));
            // r1 <= r2: um ponto nao ultrapassa o outro
            nr = (h.which == 1) ? std::min(nr, r2) : std::max(nr, r1);
            if (nr != *h.r || ns != *h.s) {
                *h.r = nr;
                *h.s = ns;
                changed = true;
            }
        }
        if (ImGui::IsItemHovered() || ImGui::IsItemActive()) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            ImGui::SetTooltip("(r%d, s%d) = (%d, %d)", h.which, h.which, *h.r, *h.s);
        }
    }

    // Curva T(r): mesma funcao usada na imagem, amostrada nos 256 niveis
    ImVec2 pts[256];
    for (int r = 0; r < 256; ++r) {
        const float s = static_cast<float>(core::contrastStretchValue(static_cast<double>(r), r1, s1, r2, s2));
        pts[r] = ImVec2(toX(static_cast<float>(r)), toY(std::max(0.0f, std::min(255.0f, s))));
    }
    dl->AddPolyline(pts, 256, colCurve, ImDrawFlags_None, 2.0f);

    // Extremos fixos (0,0) e (255,255)
    const float fs = 3.0f;
    for (float v : {0.0f, 255.0f}) {
        dl->AddRectFilled(ImVec2(toX(v) - fs, toY(v) - fs), ImVec2(toX(v) + fs, toY(v) + fs), colFixed);
    }

    // Pontos de controle
    for (const Handle& h : handles) {
        const ImVec2 c(toX(static_cast<float>(*h.r)), toY(static_cast<float>(*h.s)));
        dl->AddCircleFilled(c, kHandleRadius, pointActive[h.which - 1] ? colPointActive : colPoint);
        dl->AddCircle(c, kHandleRadius, IM_COL32(0, 0, 0, 200), 0, 1.5f);
    }

    // Rotulos dos eixos
    dl->AddText(ImVec2(origin.x + 6.0f, origin.y + 4.0f), ImGui::GetColorU32(ImGuiCol_TextDisabled), "s");
    dl->AddText(ImVec2(origin.x + side - 14.0f, origin.y + side - 18.0f), ImGui::GetColorU32(ImGuiCol_TextDisabled), "r");

    ImGui::SetCursorScreenPos(ImVec2(origin.x, origin.y + side));
    ImGui::Dummy(ImVec2(0.0f, 2.0f));
    ImGui::PopID();
    return changed;
}

}
