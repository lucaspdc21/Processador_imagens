#include "ConvolutionWindow.hpp"
#include "ImageConvolution.hpp"
#include "ImageTexture.hpp"
#include "imgui.h"
#include <algorithm>
#include <cfloat>
#include <cstdio>

namespace ui {

void RenderConvolutionWindow(AppState& state) {
    ImGui::SetNextWindowSize(ImVec2(760, 700), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Filtro por convolução", &state.showConvolutionWindow)) {
        ImGui::End();
        return;
    }

    ImGui::TextWrapped("Monte uma máscara (kernel) quadrada de tamanho ímpar e aplique na imagem. "
                       "A imagem filtrada mantém o mesmo tamanho da original.");
    ImGui::Dummy(ImVec2(0, 4));

    // ---- Tamanho do filtro
    const int sizes[] = {3, 5, 7, 9, 11, 13, 15};
    char currentLabel[16];
    std::snprintf(currentLabel, sizeof(currentLabel), "%dx%d", state.kernelSize, state.kernelSize);
    ImGui::SetNextItemWidth(110);
    if (ImGui::BeginCombo("Tamanho do filtro", currentLabel)) {
        for (int s : sizes) {
            char item[16];
            std::snprintf(item, sizeof(item), "%dx%d", s, s);
            if (ImGui::Selectable(item, s == state.kernelSize)) {
                state.setKernelSize(s);
            }
        }
        ImGui::EndCombo();
    }

    const int n = state.kernelSize;
    if (static_cast<int>(state.kernelValues.size()) != n * n) {
        state.kernelValues.assign(static_cast<size_t>(n) * n, 0.0f);
    }

    // ---- Modelos prontos
    ImGui::SameLine();
    if (ImGui::Button("Identidade")) {
        core::Kernel k = core::identityKernel(n);
        state.kernelValues.assign(k.weights.begin(), k.weights.end());
    }
    ImGui::SameLine();
    if (ImGui::Button("Média (todos iguais)")) {
        core::Kernel k = core::boxKernel(n);
        state.kernelValues.assign(k.weights.begin(), k.weights.end());
    }
    ImGui::SameLine();
    if (ImGui::Button("Zerar")) {
        std::fill(state.kernelValues.begin(), state.kernelValues.end(), 0.0f);
    }

    // ---- Coeficientes
    ImGui::Dummy(ImVec2(0, 4));
    ImGui::Text("Coeficientes w(s, t). A célula destacada é o centro, w(0, 0).");
    const float cellWidth = 62.0f;
    if (ImGui::BeginTable("##kernelGrid", n,
                          ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY |
                          ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit,
                          ImVec2(0, 330))) {
        for (int c = 0; c < n; ++c) {
            ImGui::TableSetupColumn(nullptr, ImGuiTableColumnFlags_WidthFixed, cellWidth);
        }
        for (int r = 0; r < n; ++r) {
            ImGui::TableNextRow();
            for (int c = 0; c < n; ++c) {
                ImGui::TableSetColumnIndex(c);
                if (r == n / 2 && c == n / 2) {
                    ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, IM_COL32(60, 100, 160, 110));
                }
                ImGui::PushID(r * n + c);
                ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::InputFloat("##w", &state.kernelValues[static_cast<size_t>(r) * n + c], 0.0f, 0.0f, "%.4g");
                ImGui::PopID();
            }
        }
        ImGui::EndTable();
    }

    double sum = 0.0;
    for (float v : state.kernelValues) sum += v;
    ImGui::Text("Soma dos coeficientes: %.5g", sum);
    ImGui::Checkbox("Dividir pela soma dos coeficientes (filtro de média)", &state.kernelDivideBySum);

    // ---- Opções
    ImGui::SeparatorText("Opções");

    ImGui::Text("Operação:");
    ImGui::SameLine();
    ImGui::RadioButton("Convolução (máscara girada 180 graus)", &state.filterOperation, 0);
    ImGui::SameLine();
    ImGui::RadioButton("Correlação", &state.filterOperation, 1);

    ImGui::Text("Borda:");
    ImGui::SameLine();
    ImGui::RadioButton("Preencher com zeros", &state.filterBorder, 0);
    ImGui::SameLine();
    ImGui::RadioButton("Replicar a borda", &state.filterBorder, 1);

    ImGui::Text("Resultado:");
    ImGui::SameLine();
    ImGui::RadioButton("Limitar a 0-255", &state.filterResultMode, 0);
    ImGui::SameLine();
    ImGui::RadioButton("Manter valores", &state.filterResultMode, 1);
    ImGui::TextDisabled("Com 'Manter valores' (máscaras que geram negativos ou acima de 255, como bordas),");
    ImGui::TextDisabled("a imagem é normalizada só na exibição e fica acinzentada.");

    ImGui::Dummy(ImVec2(0, 6));
    ImGui::BeginDisabled(!state.hasImage);
    if (ImGui::Button("Aplicar filtro", ImVec2(-1, 0))) {
        state.applyConvolution();
        RefreshTexture(state);
    }
    ImGui::EndDisabled();
    if (!state.hasImage) {
        ImGui::TextDisabled("Carregue uma imagem para aplicar o filtro.");
    }

    ImGui::End();
}

}
