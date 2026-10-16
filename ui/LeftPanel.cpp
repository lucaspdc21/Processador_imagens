#include "LeftPanel.hpp"
#include "ImageTexture.hpp"
#include "imgui.h"
#include "tinyfiledialogs.h"
#include <string>

namespace ui {

void RenderLeftPanel(AppState& state, float width, float height) {
    ImGui::BeginChild("PainelMetodos", ImVec2(width, height), true);
    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Métodos e funções");
    ImGui::Separator();

    if (ImGui::Button("Selecionar imagem", ImVec2(-1, 0))) {
        const char* filters[] = { "*.png", "*.jpg", "*.jpeg", "*.bmp", "*.tga" };
        const char* selected = tinyfd_openFileDialog(
            "Selecionar imagem", "", 5, filters, "Imagens", 0);
        if (selected != nullptr) {
            std::string path(selected);
            if (state.loadFromPath(path)) {
                RefreshTexture(state);
            }
        }
    }

    ImGui::BeginDisabled(!state.hasImage);

    if (ImGui::Button("Salvar como PNG", ImVec2(-1, 0))) {
        const char* filters[] = { "*.png" };
        const char* savePath = tinyfd_saveFileDialog(
            "Salvar imagem", "saida.png", 1, filters, "PNG");
        if (savePath != nullptr) {
            std::string path(savePath);
            state.saveToPath(path);
        }
    }

    ImGui::Dummy(ImVec2(0, 10));

    if (ImGui::CollapsingHeader("Resolução (intensidade / espacial)", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Indent();

        ImGui::Text("Resolução de intensidade (k bits)");
        ImGui::SetNextItemWidth(-1);
        ImGui::SliderInt("##bits", &state.bitsPerPixel, 1, 8, "k = %d bits");
        ImGui::Text("L = 2^k = %lld níveis", 1LL << state.bitsPerPixel);
        if (ImGui::Button("Aplicar quantização", ImVec2(-1, 0))) {
            state.applyQuantization();
            RefreshTexture(state);
        }

        ImGui::Dummy(ImVec2(0, 10));
        ImGui::Text("Resolução espacial (reamostragem)");
        ImGui::SetNextItemWidth(-1);
        ImGui::SliderInt("##resample", &state.resamplePercent, 5, 500, "%d%% do tamanho original");
        ImGui::Text("Interpolação");
        int method = static_cast<int>(state.interpolationMethod);
        ImGui::RadioButton("Vizinho mais próximo", &method, 0);
        ImGui::RadioButton("Bilinear", &method, 1);
        ImGui::RadioButton("Bicúbica", &method, 2);
        state.interpolationMethod = static_cast<core::resampleType>(method);

        if (ImGui::Button("Aplicar reamostragem", ImVec2(-1, 0))) {
            state.applyResample();
            RefreshTexture(state);
        }

        ImGui::Dummy(ImVec2(0, 10));
        ImGui::Text("Resolução espacial (DPI)");
        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputFloat("##dpi", &state.dpiValue, 1.0f, 10.0f, "%.1f dpi")) {
            state.applyDpi(state.dpiValue);
        }

        ImGui::Unindent();
    }

    ImGui::Dummy(ImVec2(0, 6));

    // novas tranf dorpdown
    if (ImGui::CollapsingHeader("Transformações de intensidade", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Indent();

        // Negativo
        ImGui::SeparatorText("1. Negativo");
        if (ImGui::Button("Aplicar negativo", ImVec2(-1, 0))) {
            state.applyNegative();
            RefreshTexture(state);
        }

        // Brilho
        ImGui::SeparatorText("1b. Brilho");
        ImGui::SetNextItemWidth(-1);
        ImGui::SliderFloat("##brilho", &state.brightnessFactor, 0.0f, 3.0f, "k = %.2f");
        ImGui::TextDisabled("s = k * r | k > 1 clareia, k < 1 escurece");
        if (ImGui::Button("Aplicar brilho", ImVec2(-1, 0))) {
            state.applyBrightness();
            RefreshTexture(state);
        }

        // Limiarizacao
        ImGui::SeparatorText("2. Limiarização (thresholding)");
        ImGui::SetNextItemWidth(-1);
        ImGui::SliderInt("##limiar", &state.thresholdValue, 0, 255, "m = %d");
        if (ImGui::Button("Aplicar limiarização", ImVec2(-1, 0))) {
            state.applyThreshold();
            RefreshTexture(state);
        }

        // Transformacao logaritmica
        ImGui::SeparatorText("3. Transformação logarítmica");
        ImGui::Checkbox("Calcular c automaticamente", &state.logAutoConstant);
        ImGui::BeginDisabled(state.logAutoConstant);
        ImGui::SetNextItemWidth(-1);
        ImGui::SliderFloat("##logc", &state.logConstant, 0.1f, 100.0f, "c = %.2f");
        ImGui::EndDisabled();
        if (ImGui::Button("Aplicar transformação logarítmica", ImVec2(-1, 0))) {
            state.applyLogTransform();
            RefreshTexture(state);
        }

        // Transformacao de potencia (correcao de gama)
        ImGui::SeparatorText("4. Correção de gama (potência)");
        ImGui::SetNextItemWidth(-1);
        ImGui::SliderFloat("##gamma", &state.gammaValue, 0.1f, 5.0f, "gama = %.2f");
        ImGui::SetNextItemWidth(-1);
        ImGui::SliderFloat("##gammac", &state.gammaConstant, 0.1f, 3.0f, "c = %.2f");
        ImGui::TextDisabled("gama < 1 clareia | gama > 1 escurece");
        if (ImGui::Button("Aplicar correção de gama", ImVec2(-1, 0))) {
            state.applyGammaCorrection();
            RefreshTexture(state);
        }

        // Alargamento de contraste
        ImGui::SeparatorText("5a. Alargamento de contraste");
        ImGui::TextDisabled("Pontos de controle (r1,s1) e (r2,s2)");
        ImGui::SetNextItemWidth(-1);
        ImGui::SliderInt("##r1s1", &state.stretchR1, 0, 255, "r1 = %d");
        ImGui::SetNextItemWidth(-1);
        ImGui::SliderInt("##s1", &state.stretchS1, 0, 255, "s1 = %d");
        ImGui::SetNextItemWidth(-1);
        ImGui::SliderInt("##r2", &state.stretchR2, 0, 255, "r2 = %d");
        ImGui::SetNextItemWidth(-1);
        ImGui::SliderInt("##s2", &state.stretchS2, 0, 255, "s2 = %d");
        if (ImGui::Button("Aplicar alargamento de contraste", ImVec2(-1, 0))) {
            state.applyContrastStretch();
            RefreshTexture(state);
        }

        // Fatiamento de niveis de intensidade
        ImGui::SeparatorText("5b. Fatiamento de níveis de intensidade");
        ImGui::SetNextItemWidth(-1);
        ImGui::SliderInt("##slicemin", &state.sliceMin, 0, 255, "mínimo = %d");
        ImGui::SetNextItemWidth(-1);
        ImGui::SliderInt("##slicemax", &state.sliceMax, 0, 255, "máximo = %d");
        ImGui::SetNextItemWidth(-1);
        ImGui::SliderInt("##slicehi", &state.sliceHighlight, 0, 255, "destaque = %d");
        ImGui::Checkbox("Preservar o restante da imagem", &state.slicePreserveBackground);
        if (ImGui::Button("Aplicar fatiamento de níveis", ImVec2(-1, 0))) {
            state.applyIntensitySlice();
            RefreshTexture(state);
        }

        //Plano de bits / esteganografia
        ImGui::SeparatorText("6. Plano de bits e esteganografia (LSB)");
        ImGui::SetNextItemWidth(-1);
        ImGui::SliderInt("##bitplane", &state.bitPlaneSelected, 1, 8, "plano = %d (1 = LSB, 8 = MSB)");
        if (ImGui::Button("Mostrar plano de bits selecionado", ImVec2(-1, 0))) {
            state.applyBitPlaneSlice();
            RefreshTexture(state);
        }

        ImGui::Dummy(ImVec2(0, 6));
        ImGui::Text("Esconder mensagem (usa o bit 1 / LSB)");
        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("##steganomsg", state.steganoMessageBuffer, sizeof(state.steganoMessageBuffer));
        ImGui::TextDisabled("Capacidade da imagem atual: %lld bytes", state.steganographyCapacityBytes());
        if (ImGui::Button("Esconder mensagem na imagem", ImVec2(-1, 0))) {
            if (state.embedSteganoMessage()) {
                RefreshTexture(state);
            }
        }
        if (ImGui::Button("Extrair mensagem escondida", ImVec2(-1, 0))) {
            state.extractSteganoMessage();
        }
        if (!state.steganoExtractedMessage.empty()) {
            ImGui::TextWrapped("Mensagem extraída: %s", state.steganoExtractedMessage.c_str());
        }

        // Equalizacao de histograma
        ImGui::SeparatorText("7. Equalização de histograma");
        ImGui::TextDisabled("Automática: s = (L-1) * soma de p(rj). Sem parâmetros.");
        if (ImGui::Button("Equalizar histograma", ImVec2(-1, 0))) {
            state.applyHistogramEqualization();
            RefreshTexture(state);
        }

        ImGui::Unindent();
    }

    if (ImGui::CollapsingHeader("Filtragem espacial", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Indent();

        ImGui::SeparatorText("Filtro genérico por convolução");
        ImGui::TextDisabled("Máscara quadrada de tamanho ímpar (3x3 até 15x15).");
        if (ImGui::Button("Abrir editor de filtro", ImVec2(-1, 0))) {
            state.showConvolutionWindow = true;
        }

        ImGui::Unindent();
    }

    ImGui::Dummy(ImVec2(0, 10));
    if (ImGui::Button("Comparar todas as transformações", ImVec2(-1, 0))) {
        state.compareTexturesBuilt = false;
        state.showCompareWindow = true;
    }
    ImGui::TextDisabled("Abre uma janela com uma seção por transformação, lado a lado.");

    ImGui::Dummy(ImVec2(0, 10));
    if (ImGui::Button("Restaurar imagem original", ImVec2(-1, 0))) {
        state.restoreOriginal();
        RefreshTexture(state);
    }

    ImGui::EndDisabled();
    ImGui::EndChild();
}

}
