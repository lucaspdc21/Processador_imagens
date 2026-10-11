#include "CenterPanel.hpp"
#include "ImageTexture.hpp"
#include "imgui.h"
#include <cstdint>

namespace ui {

void RenderCenterPanel(AppState& state, float width, float height) {
    ImGui::BeginChild("PainelImagem", ImVec2(width, height), true);
    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Imagem");
    ImGui::Separator();

    if (state.hasImage) {
        if (ImGui::Checkbox("Normalizar exibição (min-max)", &state.displayNormalized)) {
            RefreshTexture(state);
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(só altera a exibição)");
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
    } else {
        ImGui::TextWrapped("Nenhuma imagem carregada. Use \"Selecionar imagem...\" no painel à esquerda.");
    }

    ImGui::EndChild();
}

}
