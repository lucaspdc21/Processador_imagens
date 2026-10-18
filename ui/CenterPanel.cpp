#include "CenterPanel.hpp"
#include "imgui.h"
#include <cstdint>

namespace ui {

void RenderCenterPanel(AppState& state, float width, float height) {
    ImGui::BeginChild("PainelImagem", ImVec2(width, height), true);
    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Imagem");
    ImGui::Separator();

    if (state.hasImage) {
        ImVec2 avail = ImGui::GetContentRegionAvail();
        float imgAspect = static_cast<float>(state.workingImage.getCols()) /
                           static_cast<float>(state.workingImage.getRows());
        float drawW = avail.x;
        float drawH = drawW / imgAspect;
        if (drawH > avail.y) {
            drawH = avail.y;
            drawW = drawH * imgAspect;
        }
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (avail.x - drawW) * 0.5f);
        ImGui::Image((ImTextureID)(intptr_t)state.textureId, ImVec2(drawW, drawH));

        // Conta-gotas do chroma-key: o clique na imagem escolhe a cor-chave
        if (state.chromaPicking) {
            if (ImGui::IsItemHovered()) {
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                    ImVec2 pos = ImGui::GetItemRectMin();
                    ImVec2 mouse = ImGui::GetMousePos();
                    int col = static_cast<int>((mouse.x - pos.x) / drawW * state.workingImage.getCols());
                    int row = static_cast<int>((mouse.y - pos.y) / drawH * state.workingImage.getRows());
                    state.pickChromaColor(row, col);
                }
            }
            ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), "Clique na imagem para escolher a cor-chave.");
        }
    } else {
        ImGui::TextWrapped("Nenhuma imagem carregada. Use \"Selecionar imagem...\" no painel à esquerda.");
    }

    ImGui::EndChild();
}

}
