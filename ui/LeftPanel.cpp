#include "LeftPanel.hpp"
#include "ImageTexture.hpp"
#include "imgui.h"
#include "tinyfiledialogs.h"
#include <cstdio>
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

        // Combo de tamanho de filtro (3x3 ate 15x15)
        auto sizeCombo = [](const char* id, int& size) {
            char current[16];
            std::snprintf(current, sizeof(current), "%dx%d", size, size);
            ImGui::SetNextItemWidth(-1);
            if (ImGui::BeginCombo(id, current)) {
                for (int s = 3; s <= 15; s += 2) {
                    char item[16];
                    std::snprintf(item, sizeof(item), "%dx%d", s, s);
                    if (ImGui::Selectable(item, s == size)) size = s;
                }
                ImGui::EndCombo();
            }
        };

        ImGui::Dummy(ImVec2(0, 4));
        ImGui::Text("Borda dos filtros abaixo:");
        ImGui::RadioButton("Replicar##spBorder", &state.spatialBorder, 1);
        ImGui::SameLine();
        ImGui::RadioButton("Zeros##spBorder", &state.spatialBorder, 0);

        // ---- Suavizacao
        ImGui::SeparatorText("Suavização (média / gaussiano)");
        const char* smoothNames[] = {"Média simples", "Média ponderada (1-2-1)", "Gaussiano"};
        ImGui::SetNextItemWidth(-1);
        ImGui::Combo("##smoothType", &state.smoothType, smoothNames, 3);
        ImGui::Text("Tamanho:");
        sizeCombo("##smoothSize", state.smoothSize);
        if (state.smoothType == 2) {
            ImGui::SetNextItemWidth(-1);
            ImGui::SliderFloat("##sigma", &state.gaussianSigma, 0.3f, 6.0f, "sigma = %.2f");
        }
        if (ImGui::Button("Aplicar suavização", ImVec2(-1, 0))) {
            state.applySmoothing();
            RefreshTexture(state);
        }

        // ---- Mediana
        ImGui::SeparatorText("Filtro da mediana");
        ImGui::Text("Tamanho:");
        sizeCombo("##medianSize", state.medianSize);
        if (ImGui::Button("Aplicar mediana", ImVec2(-1, 0))) {
            state.applyMedian();
            RefreshTexture(state);
        }

        // ---- Laplaciano
        ImGui::SeparatorText("Aguçamento: Laplaciano");
        const char* lapNames[] = {"Vizinhos-4 (centro -4)", "Vizinhos-8 (centro -8)",
                                  "Vizinhos-4 (centro +4)", "Vizinhos-8 (centro +8)"};
        ImGui::SetNextItemWidth(-1);
        ImGui::Combo("##lapKind", &state.laplacianChoice, lapNames, 4);
        ImGui::TextDisabled("g = f + c * laplaciano (c = -1 se centro negativo)");
        if (ImGui::Button("Mostrar Laplaciano (normalizado)", ImVec2(-1, 0))) {
            state.showLaplacian();
            RefreshTexture(state);
        }
        if (ImGui::Button("Aguçar com Laplaciano", ImVec2(-1, 0))) {
            state.applyLaplacianSharpen();
            RefreshTexture(state);
        }

        // ---- High-boost
        ImGui::SeparatorText("Aguçamento: High-boost");
        ImGui::TextDisabled("g = f + k * (f - borrada). Usa a suavização escolhida acima.");
        ImGui::SetNextItemWidth(-1);
        ImGui::SliderFloat("##hbk", &state.highBoostK, 0.0f, 10.0f, "k = %.2f");
        if (ImGui::Button("Aplicar high-boost", ImVec2(-1, 0))) {
            state.applyHighBoost();
            RefreshTexture(state);
        }
        ImGui::TextDisabled("k = 1: máscara de nitidez | k > 1: high-boost");

        // ---- Sobel e gradiente
        ImGui::SeparatorText("Bordas: Sobel e gradiente");
        if (ImGui::Button("Sobel X", ImVec2(-1, 0))) {
            state.applySobelX();
            RefreshTexture(state);
        }
        if (ImGui::Button("Sobel Y", ImVec2(-1, 0))) {
            state.applySobelY();
            RefreshTexture(state);
        }
        ImGui::Checkbox("Aproximar por |gx| + |gy|", &state.gradientApproximate);
        if (ImGui::Button("Magnitude do gradiente", ImVec2(-1, 0))) {
            state.applyGradientMagnitude();
            RefreshTexture(state);
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
