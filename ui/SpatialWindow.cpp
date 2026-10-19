#include "SpatialWindow.hpp"
#include "ImageTexture.hpp"
#include "ImageScale.hpp"
#include "ImageRotate.hpp"
#include "imgui.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>

namespace ui {

namespace {

    // Estado da janela (vive durante toda a execucao do programa).
    struct Pane {
        float zoom = 1.0f;
        int lastRows = -1;
        int lastCols = -1;
    };

    struct SpatialUi {
        // ---- escala
        float sx = 1.0f;
        float sy = 1.0f;
        bool keepAspect = true;
        int scaleInterp = 1;            // 0: vizinho | 1: bilinear
        bool antialias = false;         // desligado por padrao, para comparar

        // ---- rotacao
        float angle = 0.0f;             // graus, positivo = anti-horario visual
        double pivotX = 0.0;
        double pivotY = 0.0;
        int rotInterp = 1;
        int canvas = 0;                 // 0: manter tamanho | 1: expandir
        int background = 0;
        bool pickPivot = false;

        // ---- resultado
        core::ColorImage result;
        unsigned int resultTexture = 0;
        bool hasResult = false;
        bool resultFromRotation = false;
        double lastMs = 0.0;
        std::string resultInfo;
        double resultOffsetX = 0.0;     // para marcar o pivo no resultado
        double resultOffsetY = 0.0;

        bool autoPreview = true;
        bool dirty = true;

        int lastRevision = -1;
        int lastRows = 0, lastCols = 0;
        int lastTab = -1;

        Pane srcPane, resPane;
    };

    SpatialUi& ui() {
        static SpatialUi s;
        return s;
    }

    // Assinatura dos parametros: se mudar, a previa fica desatualizada.
    struct ParamSig {
        int tab = 0;
        float a = 0, b = 0;
        double c = 0, d = 0;
        int e = 0, f = 0, g = 0;
        bool operator==(const ParamSig& o) const {
            return tab == o.tab && a == o.a && b == o.b && c == o.c && d == o.d &&
                   e == o.e && f == o.f && g == o.g;
        }
    };

    core::Interpolation toInterp(int v) {
        return v == 0 ? core::Interpolation::Nearest : core::Interpolation::Bilinear;
    }

    void FitZoom(Pane& pane, int rows, int cols, float paneW, float paneH) {
        if (rows <= 0 || cols <= 0) return;
        float zx = (paneW - 24.0f) / static_cast<float>(cols);
        float zy = (paneH - 60.0f) / static_cast<float>(rows);
        pane.zoom = std::max(0.02f, std::min(std::min(zx, zy), 16.0f));
    }

    // Desenha um painel de imagem com zoom, rolagem e (opcional) marcador de
    // pivo. Retorna true se o usuario clicou/arrastou para escolher um pivo,
    // preenchendo (outX, outY) em coordenadas de imagem (centro do pixel = inteiro).
    bool DrawImagePane(const char* id, const char* title, unsigned int tex, int rows, int cols,
                       Pane& pane, float paneW, float paneH, bool pickable,
                       bool showMarker, double markerX, double markerY,
                       double& outX, double& outY) {
        bool picked = false;

        ImGui::BeginGroup();
        ImGui::Text("%s: %d x %d", title, cols, rows);
        if (rows > 0 && cols > 0 && (pane.lastRows != rows || pane.lastCols != cols)) {
            FitZoom(pane, rows, cols, paneW, paneH);
            pane.lastRows = rows;
            pane.lastCols = cols;
        }

        ImGui::SetNextItemWidth(std::max(80.0f, paneW - 110.0f));
        char label[32];
        std::snprintf(label, sizeof(label), "##zoom_%s", id);
        ImGui::SliderFloat(label, &pane.zoom, 0.05f, 16.0f, "zoom %.2fx", ImGuiSliderFlags_Logarithmic);
        ImGui::SameLine();
        std::snprintf(label, sizeof(label), "Ajustar##fit_%s", id);
        if (ImGui::Button(label)) FitZoom(pane, rows, cols, paneW, paneH);

        std::snprintf(label, sizeof(label), "##pane_%s", id);
        ImGui::BeginChild(label, ImVec2(paneW, std::max(60.0f, paneH - 56.0f)), true,
                          ImGuiWindowFlags_HorizontalScrollbar);
        if (tex != 0 && rows > 0 && cols > 0) {
            const float w = static_cast<float>(cols) * pane.zoom;
            const float h = static_cast<float>(rows) * pane.zoom;
            ImGui::Image((ImTextureID)(intptr_t)tex, ImVec2(w, h));
            const ImVec2 imgMin = ImGui::GetItemRectMin();

            // Botao invisivel sobre a imagem: sem um item interativo, clicar/arrastar
            // numa Image dentro de um child move a janela inteira. Como item ativo,
            // o clique fica com a imagem (e o arraste continua mesmo fora dela).
            ImGui::SetCursorScreenPos(imgMin);
            ImGui::InvisibleButton("##hit", ImVec2(w, h));
            if (pickable && ImGui::IsItemActive()) {
                const ImVec2 mouse = ImGui::GetIO().MousePos;
                // Centro do pixel (x, y) = coordenada (x, y): a borda esquerda
                // do pixel 0 esta em -0.5.
                double x = (mouse.x - imgMin.x) / pane.zoom - 0.5;
                double y = (mouse.y - imgMin.y) / pane.zoom - 0.5;
                x = std::round(std::max(0.0, std::min(x, static_cast<double>(cols - 1))));
                y = std::round(std::max(0.0, std::min(y, static_cast<double>(rows - 1))));
                outX = x;
                outY = y;
                picked = true;
            }

            if (showMarker) {
                ImDrawList* dl = ImGui::GetWindowDrawList();
                const ImVec2 c(imgMin.x + static_cast<float>(markerX + 0.5) * pane.zoom,
                               imgMin.y + static_cast<float>(markerY + 0.5) * pane.zoom);
                const ImU32 colOuter = IM_COL32(0, 0, 0, 255);
                const ImU32 colInner = IM_COL32(255, 60, 60, 255);
                dl->AddCircle(c, 9.0f, colOuter, 24, 3.0f);
                dl->AddCircle(c, 9.0f, colInner, 24, 1.5f);
                dl->AddLine(ImVec2(c.x - 14, c.y), ImVec2(c.x + 14, c.y), colOuter, 3.0f);
                dl->AddLine(ImVec2(c.x, c.y - 14), ImVec2(c.x, c.y + 14), colOuter, 3.0f);
                dl->AddLine(ImVec2(c.x - 14, c.y), ImVec2(c.x + 14, c.y), colInner, 1.5f);
                dl->AddLine(ImVec2(c.x, c.y - 14), ImVec2(c.x, c.y + 14), colInner, 1.5f);
            }
        }
        ImGui::EndChild();
        ImGui::EndGroup();
        return picked;
    }

    // Escala e rotacao sao operacoes geometricas: a mesma transformacao (mesmo
    // tamanho de saida, mesma interpolacao) e aplicada em R, G e B e os tres
    // planos resultantes formam a nova imagem colorida. Em escala de cinza
    // (R = G = B) o resultado continua em cinza.
    template <class PlaneFn>
    core::ColorImage transformPlanes(core::ColorImage& src, PlaneFn fn) {
        core::ColorImage out;
        for (int c = 0; c < core::ColorImage::kChannels; ++c) out.channel(c) = fn(src.channel(c));
        return out;
    }

    void GenerateScale(AppState& state, SpatialUi& s) {
        core::ColorImage& src = state.workingImage;
        int ro = 0, co = 0;
        if (!core::computeScaledSize(src.getRows(), src.getCols(), s.sx, s.sy, ro, co)) {
            s.hasResult = false;
            s.resultInfo = "Escala inválida ou saída maior que 100 megapixels.";
            return;
        }
        auto t0 = std::chrono::steady_clock::now();
        s.result = transformPlanes(src, [&](core::Image& plane) {
            return core::scaleImage(plane, s.sx, s.sy, toInterp(s.scaleInterp), s.antialias);
        });
        auto t1 = std::chrono::steady_clock::now();
        s.lastMs = std::chrono::duration<double, std::milli>(t1 - t0).count();
        s.hasResult = s.result.getRows() > 0;
        s.resultFromRotation = false;
        if (s.hasResult) {
            s.resultTexture = UploadImageToTexture(s.result, s.resultTexture);
            char buf[160];
            std::snprintf(buf, sizeof(buf), "Escala %.3f x %.3f (%s) -> %d x %d em %.1f ms",
                          s.sx, s.sy, s.scaleInterp == 0 ? "vizinho" : "bilinear",
                          s.result.getCols(), s.result.getRows(), s.lastMs);
            s.resultInfo = buf;
        }
    }

    void GenerateRotation(AppState& state, SpatialUi& s) {
        core::ColorImage& src = state.workingImage;
        const core::RotationCanvas canvas = (s.canvas == 0) ? core::RotationCanvas::KeepSize
                                                             : core::RotationCanvas::Expand;
        core::RotationLayout layout;
        if (!core::computeRotationLayout(src.getRows(), src.getCols(), s.angle, s.pivotX, s.pivotY,
                                         canvas, layout)) {
            s.hasResult = false;
            s.resultInfo = "Parâmetros inválidos ou saída maior que 100 megapixels.";
            return;
        }
        auto t0 = std::chrono::steady_clock::now();
        s.result = transformPlanes(src, [&](core::Image& plane) {
            return core::rotateImage(plane, s.angle, s.pivotX, s.pivotY, toInterp(s.rotInterp),
                                     canvas, s.background);
        });
        auto t1 = std::chrono::steady_clock::now();
        s.lastMs = std::chrono::duration<double, std::milli>(t1 - t0).count();
        s.hasResult = s.result.getRows() > 0;
        s.resultFromRotation = true;
        s.resultOffsetX = layout.offsetX;
        s.resultOffsetY = layout.offsetY;
        if (s.hasResult) {
            s.resultTexture = UploadImageToTexture(s.result, s.resultTexture);
            char buf[200];
            std::snprintf(buf, sizeof(buf), "Rotação %.2f° em torno de (%.1f, %.1f) (%s) -> %d x %d em %.1f ms",
                          s.angle, s.pivotX, s.pivotY, s.rotInterp == 0 ? "vizinho" : "bilinear",
                          s.result.getCols(), s.result.getRows(), s.lastMs);
            s.resultInfo = buf;
        }
    }

    void Tooltip(const char* text) {
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", text);
    }

}

void RenderSpatialWindow(AppState& state) {
    SpatialUi& s = ui();

    ImGui::SetNextWindowSize(ImVec2(1040, 740), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Escala e rotação", &state.showSpatialWindow)) {
        ImGui::End();
        return;
    }

    if (!state.hasImage) {
        ImGui::TextWrapped("Carregue uma imagem para usar escala e rotação.");
        ImGui::End();
        return;
    }

    core::ColorImage& src = state.workingImage;
    const int rows = src.getRows();
    const int cols = src.getCols();

    // A imagem de trabalho mudou (carregada, filtrada, resultado aplicado...):
    // recentraliza o pivo e descarta a previa antiga.
    if (s.lastRevision != state.imageRevision || s.lastRows != rows || s.lastCols != cols) {
        s.lastRevision = state.imageRevision;
        s.lastRows = rows;
        s.lastCols = cols;
        core::defaultPivot(rows, cols, s.pivotX, s.pivotY);
        s.hasResult = false;
        s.resultInfo.clear();
        s.dirty = true;
        s.autoPreview = static_cast<long long>(rows) * cols <= 2000000LL;
    }

    ImGui::TextWrapped("Entrada = imagem de trabalho atual. Gere a prévia, compare lado a lado e, se gostar, "
                       "aplique o resultado à imagem de trabalho. Mapeamento inverso (sem buracos), "
                       "sem rotinas prontas de reamostragem.");

    ParamSig sig;
    int currentTab = 0;

    if (ImGui::BeginTabBar("##spatialTabs")) {
        // ------------------------------------------------------------- ESCALA
        if (ImGui::BeginTabItem("Escala")) {
            currentTab = 0;

            float oldSx = s.sx, oldSy = s.sy;
            ImGui::SetNextItemWidth(130);
            ImGui::InputFloat("sx (horizontal)", &s.sx, 0.05f, 0.5f, "%.3f");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(130);
            ImGui::InputFloat("sy (vertical)", &s.sy, 0.05f, 0.5f, "%.3f");
            ImGui::SameLine();
            ImGui::Checkbox("Manter proporção", &s.keepAspect);
            if (s.keepAspect) {
                if (s.sx != oldSx) s.sy = s.sx;
                else if (s.sy != oldSy) s.sx = s.sy;
            }
            if (ImGui::Button("x0,5")) { s.sx = 0.5f; if (s.keepAspect) s.sy = 0.5f; }
            ImGui::SameLine();
            if (ImGui::Button("x1")) { s.sx = 1.0f; if (s.keepAspect) s.sy = 1.0f; }
            ImGui::SameLine();
            if (ImGui::Button("x2")) { s.sx = 2.0f; if (s.keepAspect) s.sy = 2.0f; }
            ImGui::SameLine();
            if (ImGui::Button("x4")) { s.sx = 4.0f; if (s.keepAspect) s.sy = 4.0f; }
            ImGui::SameLine();
            ImGui::TextDisabled("(< 1 reduz, > 1 amplia)");

            ImGui::Text("Interpolação:");
            ImGui::SameLine();
            ImGui::RadioButton("Vizinho mais próximo##sc", &s.scaleInterp, 0);
            ImGui::SameLine();
            ImGui::RadioButton("Bilinear##sc", &s.scaleInterp, 1);
            ImGui::SameLine();
            ImGui::Checkbox("Suavizar antes de reduzir", &s.antialias);
            Tooltip("Aplica um filtro de média (caixa, ~ceil(1/s) por eixo) antes de reduzir,\n"
                    "para atenuar o aliasing. Desligado por padrão para você poder comparar.");

            int ro = 0, co = 0;
            if (core::computeScaledSize(rows, cols, s.sx, s.sy, ro, co)) {
                ImGui::Text("Tamanho de saída: %d x %d  (entrada: %d x %d)  |  escala efetiva: %.4f x %.4f",
                            co, ro, cols, rows,
                            static_cast<double>(co) / cols, static_cast<double>(ro) / rows);
            } else {
                ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.3f, 1.0f),
                                   "Escala inválida ou saída acima de 100 megapixels.");
            }

            sig.tab = 0;
            sig.a = s.sx; sig.b = s.sy;
            sig.e = s.scaleInterp; sig.f = s.antialias ? 1 : 0;
            ImGui::EndTabItem();
        }

        // ----------------------------------------------------------- ROTACAO
        if (ImGui::BeginTabItem("Rotação")) {
            currentTab = 1;

            ImGui::SetNextItemWidth(160);
            ImGui::InputFloat("Ângulo (graus)", &s.angle, 1.0f, 15.0f, "%.2f");
            Tooltip("Positivo = sentido anti-horário (visual). Aceita decimais e negativos.");
            ImGui::SameLine();
            if (ImGui::Button("-90")) s.angle -= 90.0f;
            ImGui::SameLine();
            if (ImGui::Button("+90")) s.angle += 90.0f;
            ImGui::SameLine();
            if (ImGui::Button("180")) s.angle += 180.0f;
            ImGui::SameLine();
            if (ImGui::Button("Zerar##ang")) s.angle = 0.0f;

            ImGui::Text("Interpolação:");
            ImGui::SameLine();
            ImGui::RadioButton("Vizinho mais próximo##rt", &s.rotInterp, 0);
            ImGui::SameLine();
            ImGui::RadioButton("Bilinear##rt", &s.rotInterp, 1);

            // ---- pivo
            ImGui::SeparatorText("Pivô");
            float pxF = static_cast<float>(s.pivotX), pyF = static_cast<float>(s.pivotY);
            ImGui::SetNextItemWidth(130);
            if (ImGui::InputFloat("px (coluna)", &pxF, 1.0f, 10.0f, "%.2f")) s.pivotX = pxF;
            ImGui::SameLine();
            ImGui::SetNextItemWidth(130);
            if (ImGui::InputFloat("py (linha)", &pyF, 1.0f, 10.0f, "%.2f")) s.pivotY = pyF;
            ImGui::SameLine();
            if (ImGui::Button("Centro da imagem")) core::defaultPivot(rows, cols, s.pivotX, s.pivotY);
            ImGui::SameLine();
            if (ImGui::Checkbox("Clicar na imagem de entrada para definir o pivô", &s.pickPivot)) {
                if (s.pickPivot && s.canvas == 1) {
                    s.canvas = 0;   // com o pivo escolhido, "manter tamanho" e a opcao que o mostra
                    state.statusMessage = "Tela de saída ajustada para 'manter tamanho': só nela o pivô muda o resultado.";
                }
            }
            ImGui::TextDisabled("Coordenadas da imagem original: x = coluna, y = linha, centro do pixel = inteiro. "
                                "Padrão: centro, ((W-1)/2, (H-1)/2).");

            // ---- tela de saida
            ImGui::SeparatorText("Tela de saída");
            ImGui::RadioButton("Manter o tamanho original", &s.canvas, 0);
            Tooltip("Parte da imagem pode ser cortada. Aqui o pivô muda o resultado.");
            ImGui::SameLine();
            ImGui::RadioButton("Expandir para conter a imagem inteira", &s.canvas, 1);
            Tooltip("O resultado é o mesmo para qualquer pivô (muda só a posição, que é\n"
                    "recentralizada pelo retângulo envolvente). O pivô só altera a imagem\n"
                    "de fato na opção 'Manter o tamanho original'.");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(150);
            ImGui::SliderInt("Fundo (0-255)", &s.background, 0, 255);

            double cx, cy;
            core::defaultPivot(rows, cols, cx, cy);
            const bool pivotOffCenter = std::fabs(s.pivotX - cx) > 1e-9 || std::fabs(s.pivotY - cy) > 1e-9;
            if (s.canvas == 1 && pivotOffCenter) {
                ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f),
                                   "Em 'Expandir' o pivô não altera a imagem. Use 'Manter o tamanho original' para ver o efeito do pivô.");
            }

            core::RotationLayout layout;
            const core::RotationCanvas cv = (s.canvas == 0) ? core::RotationCanvas::KeepSize
                                                             : core::RotationCanvas::Expand;
            if (core::computeRotationLayout(rows, cols, s.angle, s.pivotX, s.pivotY, cv, layout)) {
                ImGui::Text("Tamanho de saída: %d x %d  (entrada: %d x %d)", layout.outCols, layout.outRows, cols, rows);
            } else {
                ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.3f, 1.0f),
                                   "Parâmetros inválidos ou saída acima de 100 megapixels.");
            }

            sig.tab = 1;
            sig.a = s.angle; sig.c = s.pivotX; sig.d = s.pivotY;
            sig.e = s.rotInterp; sig.f = s.canvas; sig.g = s.background;
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    // Trocar de aba invalida a previa (ela era de outra operacao).
    if (s.lastTab != currentTab) {
        if (s.lastTab != -1) { s.hasResult = false; s.resultInfo.clear(); }
        s.lastTab = currentTab;
        s.dirty = true;
    }

    // ---- acoes
    static ParamSig lastSig;
    if (!(lastSig == sig)) {
        lastSig = sig;
        s.dirty = true;
    }

    ImGui::Dummy(ImVec2(0, 2));
    ImGui::Checkbox("Prévia automática", &s.autoPreview);
    Tooltip("Recalcula o resultado a cada mudança de parâmetro.\n"
            "Desligue para imagens grandes e use 'Gerar prévia'.");
    ImGui::SameLine();
    const bool wantGenerate = ImGui::Button("Gerar prévia");
    ImGui::SameLine();
    ImGui::BeginDisabled(!s.hasResult);
    const bool wantApply = ImGui::Button("Aplicar resultado à imagem de trabalho");
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (s.dirty && s.hasResult) ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "(prévia desatualizada)");

    if (wantGenerate || (s.autoPreview && s.dirty)) {
        if (currentTab == 0) GenerateScale(state, s);
        else GenerateRotation(state, s);
        s.dirty = false;
    }

    if (wantApply && s.hasResult) {
        state.workingImage = s.result;
        state.statusMessage = s.resultInfo + " (aplicado à imagem de trabalho).";
        RefreshTexture(state);       // incrementa a revisao: pivo e previa sao reiniciados
        s.hasResult = false;
        ImGui::End();
        return;
    }

    if (!s.resultInfo.empty()) ImGui::TextWrapped("%s", s.resultInfo.c_str());

    // ---- visualizacao lado a lado
    ImVec2 avail = ImGui::GetContentRegionAvail();
    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    const float paneW = std::max(120.0f, (avail.x - spacing) * 0.5f);
    const float paneH = std::max(140.0f, avail.y);

    double pickedX = s.pivotX, pickedY = s.pivotY;
    const bool pivotMode = (currentTab == 1);
    const bool picked = DrawImagePane("src", "Entrada", state.textureId, rows, cols, s.srcPane,
                                      paneW, paneH, pivotMode && s.pickPivot,
                                      pivotMode, s.pivotX, s.pivotY, pickedX, pickedY);
    if (picked) {
        s.pivotX = pickedX;
        s.pivotY = pickedY;
    }
    ImGui::SameLine();

    double dummyX = 0, dummyY = 0;
    const bool showResultMarker = pivotMode && s.hasResult && s.resultFromRotation;
    // O pivo e ponto fixo da rotacao: na saida ele fica em p - o.
    DrawImagePane("res", "Resultado", s.hasResult ? s.resultTexture : 0,
                  s.hasResult ? s.result.getRows() : 0, s.hasResult ? s.result.getCols() : 0,
                  s.resPane, paneW, paneH, false,
                  showResultMarker, s.pivotX - s.resultOffsetX, s.pivotY - s.resultOffsetY,
                  dummyX, dummyY);

    ImGui::End();
}

}
