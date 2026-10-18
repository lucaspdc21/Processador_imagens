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

    // ---- Imagens coloridas ----
    if (ImGui::CollapsingHeader("Cor: sistemas RGB / HSI, cinza e sépia")) {
        ImGui::Indent();

        ImGui::SeparatorText("Sistemas de cores (RGB <-> HSI)");
        ImGui::TextDisabled("H = matiz (graus), S = saturação, I = (R+G+B)/3");
        const char* viewNames[] = {"Imagem colorida (RGB)", "Plano R", "Plano G", "Plano B",
                                   "Plano H (matiz)", "Plano S (saturação)", "Plano I (intensidade)"};
        ImGui::SetNextItemWidth(-1);
        if (ImGui::Combo("##viewMode", &state.viewChoice, viewNames, 7)) {
            RefreshTexture(state);
        }
        ImGui::TextDisabled("Só muda a exibição. H e S não têm sentido em cinza (S = 0).");
        ImGui::BeginDisabled(state.viewChoice == 0);
        if (ImGui::Button("Usar plano exibido como imagem", ImVec2(-1, 0))) {
            state.applyViewToImage();
            RefreshTexture(state);
        }
        ImGui::EndDisabled();

        ImGui::SeparatorText("Escala de cinza");
        if (ImGui::Button("Cinza: média aritmética (R+G+B)/3", ImVec2(-1, 0))) {
            state.applyGrayAverage();
            RefreshTexture(state);
        }
        ImGui::Dummy(ImVec2(0, 4));
        ImGui::Text("Média ponderada: Y = wR R + wG G + wB B");
        ImGui::SetNextItemWidth(-1);
        ImGui::SliderFloat("##gwr", &state.grayWeightR, 0.0f, 1.0f, "wR = %.3f");
        ImGui::SetNextItemWidth(-1);
        ImGui::SliderFloat("##gwg", &state.grayWeightG, 0.0f, 1.0f, "wG = %.3f");
        ImGui::SetNextItemWidth(-1);
        ImGui::SliderFloat("##gwb", &state.grayWeightB, 0.0f, 1.0f, "wB = %.3f");
        ImGui::TextDisabled("Soma dos pesos = %.3f (normalizada ao aplicar)",
                            state.grayWeightR + state.grayWeightG + state.grayWeightB);
        if (ImGui::Button("ITU-R BT.601 (0.299 / 0.587 / 0.114)", ImVec2(-1, 0))) {
            state.grayWeightR = 0.299f;
            state.grayWeightG = 0.587f;
            state.grayWeightB = 0.114f;
        }
        if (ImGui::Button("Cinza: média ponderada", ImVec2(-1, 0))) {
            state.applyGrayWeighted();
            RefreshTexture(state);
        }

        ImGui::SeparatorText("Sépia (cinza amarelado)");
        const char* sepiaNames[] = {"Matriz clássica", "Via HSI (matiz e saturação fixos)"};
        ImGui::SetNextItemWidth(-1);
        ImGui::Combo("##sepiaMode", &state.sepiaMode, sepiaNames, 2);
        if (state.sepiaMode == 1) {
            ImGui::SetNextItemWidth(-1);
            ImGui::SliderFloat("##sepiaH", &state.sepiaHue, 20.0f, 50.0f, "H = %.0f°");
            ImGui::SetNextItemWidth(-1);
            ImGui::SliderFloat("##sepiaS", &state.sepiaSaturation, 0.0f, 0.8f, "S = %.2f");
        }
        if (ImGui::Button("Aplicar sépia", ImVec2(-1, 0))) {
            state.applySepia();
            RefreshTexture(state);
        }

        ImGui::Unindent();
    }

    if (ImGui::CollapsingHeader("Cor: ajustes e chroma-key")) {
        ImGui::Indent();

        // ---- Matiz, saturacao e brilho
        ImGui::SeparatorText("Matiz, saturação e brilho (HSI)");
        ImGui::SetNextItemWidth(-1);
        ImGui::SliderFloat("##hsiH", &state.hsiDeltaHue, -180.0f, 180.0f, "matiz: %+.0f°");
        ImGui::SetNextItemWidth(-1);
        ImGui::SliderFloat("##hsiS", &state.hsiSaturationFactor, 0.0f, 2.0f, "saturação: x %.2f");
        ImGui::SetNextItemWidth(-1);
        ImGui::SliderFloat("##hsiI", &state.hsiIntensityFactor, 0.0f, 2.0f, "brilho (I): x %.2f");
        ImGui::TextDisabled("H' = H + dH | S' = k_s * S | I' = k * I (cada um independente)");
        if (ImGui::Button("Aplicar ajuste HSI", ImVec2(-1, 0))) {
            state.applyHsiAdjust();
            RefreshTexture(state);
        }
        if (ImGui::Button("Zerar ajuste HSI", ImVec2(-1, 0))) {
            state.hsiDeltaHue = 0.0f;
            state.hsiSaturationFactor = 1.0f;
            state.hsiIntensityFactor = 1.0f;
        }

        // ---- Canais complementares
        ImGui::SeparatorText("Ajuste de canal (C/R, M/G, Y/B)");
        ImGui::SetNextItemWidth(-1);
        ImGui::SliderInt("##dR", &state.channelDeltaR, -255, 255, "Ciano <-> Vermelho: %+d");
        ImGui::SetNextItemWidth(-1);
        ImGui::SliderInt("##dG", &state.channelDeltaG, -255, 255, "Magenta <-> Verde: %+d");
        ImGui::SetNextItemWidth(-1);
        ImGui::SliderInt("##dB", &state.channelDeltaB, -255, 255, "Amarelo <-> Azul: %+d");
        ImGui::TextDisabled("Cada cor é complementar da outra: mais R = menos C (C = 1 - R).");
        if (ImGui::Button("Aplicar ajuste de canal", ImVec2(-1, 0))) {
            state.applyChannelAdjust();
            RefreshTexture(state);
        }
        if (ImGui::Button("Zerar ajuste de canal", ImVec2(-1, 0))) {
            state.channelDeltaR = state.channelDeltaG = state.channelDeltaB = 0;
        }

        // ---- Chroma-key
        ImGui::SeparatorText("Chroma-key");
        ImGui::TextDisabled("A imagem atual (fundo verde) é a frente.");
        if (ImGui::Button("Escolher imagem de fundo...", ImVec2(-1, 0))) {
            const char* filters[] = { "*.png", "*.jpg", "*.jpeg", "*.bmp", "*.tga" };
            const char* selected = tinyfd_openFileDialog(
                "Imagem que substituirá o verde", "", 5, filters, "Imagens", 0);
            if (selected != nullptr) {
                std::string path(selected);
                state.loadChromaBackground(path);
            }
        }
        if (state.hasChromaBackground) {
            ImGui::TextWrapped("Fundo: %s (%d x %d)", state.chromaBackgroundName.c_str(),
                               state.chromaBackground.getCols(), state.chromaBackground.getRows());
        } else {
            ImGui::TextDisabled("Nenhuma imagem de fundo escolhida.");
        }

        ImGui::Text("Cor-chave a:");
        ImGui::SetNextItemWidth(-1);
        ImGui::ColorEdit3("##chromaColor", state.chromaKeyColor, ImGuiColorEditFlags_Uint8);
        if (ImGui::Button(state.chromaPicking ? "Cancelar conta-gotas" : "Conta-gotas (clicar na imagem)", ImVec2(-1, 0))) {
            state.chromaPicking = !state.chromaPicking;
        }
        ImGui::SetNextItemWidth(-1);
        ImGui::SliderInt("##pickRadius", &state.chromaPickRadius, 0, 20, "média numa janela de raio %d px");
        if (ImGui::Button("Verde puro (0, 255, 0)", ImVec2(-1, 0))) {
            state.chromaKeyColor[0] = 0.0f;
            state.chromaKeyColor[1] = 1.0f;
            state.chromaKeyColor[2] = 0.0f;
        }

        const char* chromaModes[] = {"Esfera: distância euclidiana", "Cubo: |zj - aj| <= D0 em cada canal"};
        ImGui::SetNextItemWidth(-1);
        ImGui::Combo("##chromaMode", &state.chromaMode, chromaModes, 2);
        ImGui::SetNextItemWidth(-1);
        ImGui::SliderFloat("##chromaTol", &state.chromaTolerance, 0.0f, 100.0f, "distância D0 = %.1f%%");
        ImGui::TextDisabled("D(z, a) <= D0: pixel vira o fundo. 100%% = distância máxima do RGB.");
        ImGui::SetNextItemWidth(-1);
        ImGui::SliderFloat("##chromaFeather", &state.chromaFeather, 0.0f, 30.0f, "borda suave = %.1f%%");
        if (ImGui::Button("Aplicar chroma-key", ImVec2(-1, 0))) {
            state.applyChromaKey();
            RefreshTexture(state);
        }

        ImGui::Unindent();
    }

    // novas tranf dorpdown
    if (ImGui::CollapsingHeader("Transformações de intensidade", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Indent();

        // Em imagens coloridas: em quais componentes aplicar
        ImGui::SeparatorText("Imagem colorida: aplicar em");
        const char* targetNames[] = {"R, G e B (cada canal)", "Só no componente I (HSI)"};
        ImGui::SetNextItemWidth(-1);
        ImGui::Combo("##colorTarget", &state.colorTargetChoice, targetNames, 2);
        ImGui::TextDisabled("Vale para as transformações abaixo e para os filtros. O negativo é sempre em RGB.");

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
        ImGui::TextDisabled("Capacidade da imagem atual: %lld bytes (LSB do canal R)", state.steganographyCapacityBytes());
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
        ImGui::TextDisabled("s = (L-1) * soma de p(rj), aplicada só ao componente I (HSI):");
        ImGui::TextDisabled("as cores (H e S) são preservadas.");
        ImGui::SetNextItemWidth(-1);
        ImGui::SliderFloat("##eqSat", &state.equalizeSaturationGain, 0.5f, 2.0f, "saturação depois: x %.2f");
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
        ImGui::Checkbox("Cor: gradiente vetorial (Di Zenzo)", &state.gradientDiZenzo);
        ImGui::TextDisabled("Desmarcado: magnitude em cada canal, somadas.");
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
