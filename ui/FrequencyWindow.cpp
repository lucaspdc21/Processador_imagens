#include "FrequencyWindow.hpp"
#include "ImageTexture.hpp"
#include "RgbaTexture.hpp"
#include "ImageColor.hpp"
#include "FrequencySession.hpp"
#include "FrequencyFilters.hpp"
#include "imgui.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <string>
#include <thread>
#include <vector>

namespace ui {

namespace {

    // ---------------------------------------------------------------------
    // Trabalho em segundo plano (DFT e IDFT podem demorar): a GUI nao congela,
    // mostra progresso e permite cancelar. Enquanto o worker roda, a sessao
    // NAO e tocada pela interface.
    // ---------------------------------------------------------------------
    struct Worker {
        enum class Kind { None, Forward, Inverse };

        std::thread thread;
        std::atomic<bool> running{false};
        core::TransformControl control;
        Kind kind = Kind::None;
        bool ok = false;
        double ms = 0.0;
        core::Image input{0, 0};     // copia da imagem de entrada (o worker nao toca na original)

        ~Worker() {
            control.cancel = true;
            if (thread.joinable()) thread.join();
        }
    };

    struct Pane {
        float zoom = 1.0f;
        int lastRows = -1;
        int lastCols = -1;
    };

    struct FreqUi {
        core::FrequencySession session;
        Worker worker;

        // transformada
        int algo = 1;                       // indice: 0 DFT propria | 1 FFT propria | 2 biblioteca
        bool padding = true;
        int normalization = 0;              // 0: min-max | 1: recortar
        int displayMode = 0;                // 0 espectro filtrado | 1 original | 2 mascara | 3 fase
        bool confirmSlowDft = false;

        // ferramentas
        int tool = 0;                       // 0: pincel | 1: notch por clique
        int brushColor = 0;                 // 0 preto | 1 branco | 2 cinza
        int gray = 128;
        bool hardDisc = false;
        float radius = 12.0f;
        float hardness = 0.5f;
        float opacity = 1.0f;
        bool liveIdft = false;              // recalcula ao soltar o mouse
        bool painting = false;
        ImVec2 lastPos{0, 0};

        // filtros pre-definidos
        int presetKind = 0;
        int presetShape = 2;
        float presetD0 = 30.0f;
        int presetOrder = 2;
        float presetWidth = 10.0f;
        std::vector<core::Notch> notches;
        float notchClickD0 = 6.0f;
        int notchClickShape = 1;            // Butterworth

        // resultado / texturas
        core::Image result{0, 0};
        bool hasResult = false;
        unsigned int spectrumTexture = 0;
        int dispRows = 0, dispCols = 0;     // P x Q do espectro exibido (valido mesmo com o worker rodando)
        unsigned int resultTexture = 0;
        int computedRevision = -1;          // imageRevision da entrada da DFT
        std::string info;
        std::string error;
        double dftMs = 0.0, idftMs = 0.0;

        Pane inPane, specPane, resPane;
    };

    FreqUi& ui() {
        static FreqUi s;
        return s;
    }

    const core::TransformAlgorithm kAlgos[3] = {
        core::TransformAlgorithm::NaiveDFT,
        core::TransformAlgorithm::RadixFFT,
        core::TransformAlgorithm::LibraryFFT};

    const core::FilterKind kKinds[6] = {
        core::FilterKind::LowPass, core::FilterKind::HighPass,
        core::FilterKind::BandReject, core::FilterKind::BandPass,
        core::FilterKind::NotchReject, core::FilterKind::NotchPass};

    const core::FilterShape kShapes[3] = {
        core::FilterShape::Ideal, core::FilterShape::Butterworth, core::FilterShape::Gaussian};

    void Tooltip(const char* text) {
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", text);
    }

    void FitZoom(Pane& pane, int rows, int cols, float paneW, float paneH) {
        if (rows <= 0 || cols <= 0) return;
        const float zx = (paneW - 24.0f) / static_cast<float>(cols);
        const float zy = (paneH - 60.0f) / static_cast<float>(rows);
        pane.zoom = std::max(0.02f, std::min(std::min(zx, zy), 16.0f));
    }

    struct PaneEvents {
        bool hovered = false;
        bool active = false;
        bool clicked = false;
        ImVec2 imgMin{0, 0};
    };

    // Painel de imagem com zoom e rolagem. 'overlay' desenha por cima da imagem.
    PaneEvents DrawPane(const char* id, const char* title, unsigned int tex, int rows, int cols,
                        Pane& pane, float paneW, float paneH,
                        const std::function<void(ImDrawList*, const PaneEvents&, float)>& overlay) {
        PaneEvents ev;
        ImGui::BeginGroup();
        ImGui::Text("%s: %d x %d", title, cols, rows);
        if (rows > 0 && cols > 0 && (pane.lastRows != rows || pane.lastCols != cols)) {
            FitZoom(pane, rows, cols, paneW, paneH);
            pane.lastRows = rows;
            pane.lastCols = cols;
        }

        char label[48];
        ImGui::SetNextItemWidth(std::max(60.0f, paneW - 100.0f));
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
            ev.imgMin = ImGui::GetItemRectMin();
            // Item interativo sobre a imagem: o clique/arraste fica com a imagem
            // (sem ele, arrastar numa Image dentro de um child move a janela).
            ImGui::SetCursorScreenPos(ev.imgMin);
            ImGui::InvisibleButton("##hit", ImVec2(w, h));
            ev.hovered = ImGui::IsItemHovered();
            ev.active = ImGui::IsItemActive();
            ev.clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
            if (overlay) overlay(ImGui::GetWindowDrawList(), ev, pane.zoom);
        }
        ImGui::EndChild();
        ImGui::EndGroup();
        return ev;
    }

    // ---------------------------------------------------------------------
    // Acoes
    // ---------------------------------------------------------------------
    void RefreshSpectrumFull(FreqUi& f) {
        auto& s = f.session;
        if (!s.hasTransform()) return;
        s.renderAll();
        f.spectrumTexture = UploadRgbaFull(f.spectrumTexture, s.displayRGBA().data(), s.P(), s.Q());
        f.dispRows = s.P();
        f.dispCols = s.Q();
    }

    void RefreshSpectrumRect(FreqUi& f, const core::Rect& r) {
        auto& s = f.session;
        if (!s.hasTransform() || r.empty()) return;
        s.renderRect(r);
        UploadRgbaRect(f.spectrumTexture, s.displayRGBA().data(), s.Q(), r);
    }

    void StartForward(AppState& state, FreqUi& f) {
        Worker& w = f.worker;
        if (w.running.load()) return;
        if (w.thread.joinable()) w.thread.join();
        w.input = core::intensityPlane(state.workingImage);   // plano I = (R+G+B)/3 (copia para o worker)
        w.kind = Worker::Kind::Forward;
        w.control.cancel = false;
        w.control.progress = 0.0f;
        w.ok = false;
        w.running = true;
        const core::TransformAlgorithm algo = kAlgos[f.algo];
        const bool pad = f.padding;
        f.computedRevision = state.imageRevision;
        f.error.clear();
        w.thread = std::thread([&f, algo, pad]() {
            const auto t0 = std::chrono::steady_clock::now();
            f.worker.ok = f.session.compute(f.worker.input, algo, pad, &f.worker.control);
            f.worker.ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
            f.worker.running = false;
        });
    }

    void StartInverse(FreqUi& f) {
        Worker& w = f.worker;
        if (w.running.load() || !f.session.hasTransform()) return;
        if (w.thread.joinable()) w.thread.join();
        w.kind = Worker::Kind::Inverse;
        w.control.cancel = false;
        w.control.progress = 0.0f;
        w.ok = false;
        w.running = true;
        f.error.clear();
        w.thread = std::thread([&f]() {
            const auto t0 = std::chrono::steady_clock::now();
            // mesmo algoritmo com que a DFT foi calculada (P e Q compativeis)
            f.worker.ok = f.session.filter(f.session.algorithm(), &f.worker.control);
            f.worker.ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
            f.worker.running = false;
        });
    }

    void ShowResult(FreqUi& f) {
        if (!f.session.hasResult()) return;
        f.result = f.session.resultImage(f.normalization == 0 ? core::OutputNormalization::MinMax
                                                              : core::OutputNormalization::Clip);
        core::ColorImage resultView(f.result);        // cinza -> R = G = B, so para exibir
        f.resultTexture = UploadImageToTexture(resultView, f.resultTexture);
        f.hasResult = true;
    }

    // Chamado no thread principal quando o worker termina.
    void FinishJob(AppState& state, FreqUi& f) {
        Worker& w = f.worker;
        if (w.thread.joinable()) w.thread.join();
        char buf[256];
        if (w.kind == Worker::Kind::Forward) {
            if (w.ok) {
                f.dftMs = w.ms;
                f.hasResult = false;
                f.session.setDisplayMode(static_cast<core::DisplayMode>(f.displayMode));
                RefreshSpectrumFull(f);
                std::snprintf(buf, sizeof(buf), "DFT calculada (%s): %d x %d -> %d x %d em %.0f ms.",
                              core::algorithmName(f.session.algorithm()), f.session.M(), f.session.N(),
                              f.session.P(), f.session.Q(), w.ms);
                f.info = buf;
                state.statusMessage = f.info;
            } else {
                f.error = f.session.lastError();
                state.statusMessage = "DFT: " + f.error;
            }
        } else if (w.kind == Worker::Kind::Inverse) {
            if (w.ok) {
                f.idftMs = w.ms;
                ShowResult(f);
                std::snprintf(buf, sizeof(buf), "Filtragem aplicada (IDFT) em %.0f ms; max|Im|/max|Re| = %.2e.",
                              w.ms, f.session.residualImag());
                f.info = buf;
                state.statusMessage = f.info;
            } else {
                f.error = f.session.lastError();
                state.statusMessage = "IDFT: " + f.error;
            }
        }
        w.kind = Worker::Kind::None;
    }

    core::FilterParams PresetParams(const FreqUi& f) {
        core::FilterParams p;
        p.kind = kKinds[f.presetKind];
        p.shape = kShapes[f.presetShape];
        p.d0 = f.presetD0;
        p.order = f.presetOrder;
        p.width = f.presetWidth;
        p.notches = f.notches;
        return p;
    }

    core::Brush CurrentBrush(const FreqUi& f) {
        core::Brush b;
        b.radius = f.radius;
        b.hardness = f.hardness;
        b.opacity = f.opacity;
        b.hardDisc = f.hardDisc;
        b.value = (f.brushColor == 0) ? 0.0 : (f.brushColor == 1 ? 1.0 : f.gray / 255.0);
        return b;
    }

    bool LiveAllowed(const FreqUi& f) {
        const long long pq = static_cast<long long>(f.session.P()) * f.session.Q();
        return f.session.algorithm() != core::TransformAlgorithm::NaiveDFT || pq <= 128LL * 128LL;
    }

}

void RenderFrequencyWindow(AppState& state) {
    FreqUi& f = ui();
    core::FrequencySession& session = f.session;

    ImGui::SetNextWindowSize(ImVec2(1500, 860), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Filtragem no domínio da frequência", &state.showFrequencyWindow)) {
        ImGui::End();
        return;
    }

    if (!state.hasImage) {
        ImGui::TextWrapped("Carregue uma imagem para usar a filtragem no domínio da frequência.");
        ImGui::End();
        return;
    }

    // Termino do trabalho em segundo plano
    const bool busy = f.worker.running.load();
    if (!busy && f.worker.kind != Worker::Kind::None) FinishJob(state, f);

    const bool hasF = !busy && session.hasTransform();
    const int M = state.workingImage.getRows();
    const int N = state.workingImage.getCols();

    // =================================================================
    // Coluna esquerda: controles
    // =================================================================
    ImGui::BeginChild("##freqControls", ImVec2(350, 0), true);

    // ---------------------------------------------------- 1. transformada
    ImGui::SeparatorText("1. Transformada de Fourier");
    ImGui::TextDisabled("Imagem colorida: o espectro é da intensidade I = (R+G+B)/3.");
    Tooltip("Ao usar o resultado, só o plano I (HSI) é substituído:\n"
            "matiz e saturação da imagem de trabalho são preservados.");
    ImGui::BeginDisabled(busy);

    ImGui::Text("Algoritmo da transformada");
    ImGui::SetNextItemWidth(-1);
    if (ImGui::BeginCombo("##algo", core::algorithmName(kAlgos[f.algo]))) {
        for (int i = 0; i < 3; ++i) {
            const bool unavailable = (i == 2 && !core::libraryFFTAvailable());
            std::string label = core::algorithmName(kAlgos[i]);
            if (unavailable) label += " [não compilada]";
            if (ImGui::Selectable(label.c_str(), f.algo == i,
                                  unavailable ? ImGuiSelectableFlags_Disabled : ImGuiSelectableFlags_None)) {
                f.algo = i;
            }
        }
        ImGui::EndCombo();
    }
    Tooltip("DFT própria: separável O(MN(M+N)) — lenta em imagens grandes.\n"
            "FFT própria: radix-2, O(MN log MN) — exige P e Q potências de 2.\n"
            "FFT da biblioteca: pocketfft — alternativa para imagens grandes.\n"
            "Todas dão o mesmo resultado (erro relativo < 1e-9).");

    ImGui::Checkbox("Usar preenchimento (recomendado)", &f.padding);
    Tooltip("Preenche com zeros até P x Q (P >= 2M-1, Q >= 2N-1) para evitar o erro de\n"
            "wraparound da convolução circular. Desmarcado: P = M, Q = N (mais rápido,\n"
            "com wraparound; arredondado para par / potência de 2).");

    const core::PaddedSize ps = core::computePaddedSize(M, N, kAlgos[f.algo], f.padding);
    const long long pq = static_cast<long long>(ps.P) * ps.Q;
    ImGui::Text("Imagem M x N: %d x %d", M, N);
    ImGui::Text("Transformada P x Q: %d x %d", ps.P, ps.Q);

    const bool tooBig = pq > core::kMaxPaddedPixels;
    const bool naive = (f.algo == 0);
    const bool slowNaive = naive && pq > 512LL * 512LL;
    if (tooBig) {
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.3f, 1.0f),
                           "P x Q acima do limite (4096 x 4096). Reduza a imagem ou desmarque o preenchimento.");
    } else if (naive) {
        const double est = core::estimateNaiveSeconds(ps.P, ps.Q);
        if (slowNaive) {
            ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.2f, 1.0f),
                               "Tempo estimado da DFT ingênua: ~%.1f s. P x Q grande: prefira a FFT.", est);
        } else {
            ImGui::TextDisabled("Tempo estimado da DFT ingênua: ~%.2f s", est);
        }
    }

    ImGui::BeginDisabled(tooBig || (f.algo == 2 && !core::libraryFFTAvailable()));
    if (ImGui::Button("Calcular DFT", ImVec2(-1, 0))) {
        if (slowNaive) {
            ImGui::OpenPopup("Confirmar DFT ingênua");
        } else {
            StartForward(state, f);
        }
    }
    ImGui::EndDisabled();

    if (ImGui::BeginPopupModal("Confirmar DFT ingênua", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("P x Q = %d x %d com a DFT ingênua pode demorar (~%.0f s).", ps.P, ps.Q,
                    core::estimateNaiveSeconds(ps.P, ps.Q));
        ImGui::Text("A FFT é equivalente e muito mais rápida.");
        if (ImGui::Button("Usar FFT própria")) {
            f.algo = 1;
            ImGui::CloseCurrentPopup();
            StartForward(state, f);
        }
        ImGui::SameLine();
        if (ImGui::Button("Continuar com a DFT")) {
            ImGui::CloseCurrentPopup();
            StartForward(state, f);
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancelar")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    ImGui::EndDisabled();   // busy

    if (busy) {
        ImGui::ProgressBar(f.worker.control.progress.load(), ImVec2(-1, 0));
        ImGui::Text("%s...", f.worker.kind == Worker::Kind::Forward ? "Calculando a DFT" : "Calculando a transformada inversa");
        if (ImGui::Button("Cancelar operação", ImVec2(-1, 0))) f.worker.control.cancel = true;
    }

    if (!f.error.empty()) ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.3f, 1.0f), "%s", f.error.c_str());
    if (!f.info.empty()) ImGui::TextWrapped("%s", f.info.c_str());
    if (!busy && session.hasTransform() && f.computedRevision != state.imageRevision) {
        ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.2f, 1.0f),
                           "A imagem de trabalho mudou depois do cálculo da DFT. Recalcule para filtrar a imagem atual.");
    }
    if (!busy && session.hasTransform()) {
        ImGui::TextDisabled("DFT em cache: %s, %d x %d. S_max = %.3f", core::algorithmName(session.algorithm()),
                            session.P(), session.Q(), session.logMax());
    }

    // ---------------------------------------------------- 2. espectro
    ImGui::SeparatorText("2. Espectro (centralizado)");
    ImGui::BeginDisabled(!hasF);
    {
        int mode = f.displayMode;
        bool changed = false;
        changed |= ImGui::RadioButton("Espectro |H·F| (o que você riscou)", &mode, 0);
        changed |= ImGui::RadioButton("Espectro original |F|", &mode, 1);
        changed |= ImGui::RadioButton("Máscara H", &mode, 2);
        changed |= ImGui::RadioButton("Fase (informativo)", &mode, 3);
        if (changed) {
            f.displayMode = mode;
            session.setDisplayMode(static_cast<core::DisplayMode>(mode));
            RefreshSpectrumFull(f);
        }
        Tooltip("Espectro: S = log(1 + |F|), normalizado por S_max. A origem fica no centro.");
    }

    // ---------------------------------------------------- 3. edicao
    ImGui::SeparatorText("3. Edição do espectro");
    ImGui::RadioButton("Pincel", &f.tool, 0);
    ImGui::SameLine();
    ImGui::RadioButton("Notch por clique", &f.tool, 1);
    Tooltip("Pincel: risque no espectro. Cada pincelada é espelhada no ponto conjugado\n"
            "((P-u) mod P, (Q-v) mod Q), então a imagem filtrada continua real.\n"
            "Notch por clique: clique num pico para rejeitar esse pico e o simétrico.");

    if (f.tool == 0) {
        ImGui::RadioButton("Preto (remove)", &f.brushColor, 0);
        ImGui::SameLine();
        ImGui::RadioButton("Branco (restaura)", &f.brushColor, 1);
        ImGui::SameLine();
        ImGui::RadioButton("Cinza (atenua)", &f.brushColor, 2);
        ImGui::BeginDisabled(f.brushColor != 2);
        ImGui::SetNextItemWidth(-1);
        ImGui::SliderInt("##gray", &f.gray, 0, 255, "tom de cinza = %d");
        ImGui::EndDisabled();
        ImGui::Checkbox("Disco duro (em vez de pincel suave)", &f.hardDisc);
        ImGui::SetNextItemWidth(-1);
        ImGui::SliderFloat("##radius", &f.radius, 1.0f, 200.0f, "raio = %.1f px", ImGuiSliderFlags_Logarithmic);
        ImGui::BeginDisabled(f.hardDisc);
        ImGui::SetNextItemWidth(-1);
        ImGui::SliderFloat("##hardness", &f.hardness, 0.0f, 1.0f, "dureza = %.2f");
        Tooltip("Pincel suave: gaussiano com sigma = raio * (1 - 0,9 * dureza).\n"
                "Dureza alta (>= 0,75) acrescenta um núcleo plano: dureza 1 = quase um disco.");
        ImGui::EndDisabled();
        ImGui::SetNextItemWidth(-1);
        ImGui::SliderFloat("##opacity", &f.opacity, 0.05f, 1.0f, "opacidade = %.2f");
        ImGui::TextDisabled("H <- H(1 - a·op) + v·a·op   (preto remove, branco restaura)");
    } else {
        ImGui::SetNextItemWidth(-1);
        ImGui::SliderFloat("##notchd0", &f.notchClickD0, 1.0f, 100.0f, "raio do notch D0 = %.1f px", ImGuiSliderFlags_Logarithmic);
        ImGui::Text("Formato:");
        ImGui::SameLine();
        ImGui::RadioButton("Ideal##nc", &f.notchClickShape, 0);
        ImGui::SameLine();
        ImGui::RadioButton("Butterworth##nc", &f.notchClickShape, 1);
        ImGui::SameLine();
        ImGui::RadioButton("Gaussiano##nc", &f.notchClickShape, 2);
    }

    ImGui::BeginDisabled(!hasF || !session.mask().canUndo());
    if (ImGui::Button("Desfazer")) {
        RefreshSpectrumRect(f, session.mask().undo());
        if (f.liveIdft && LiveAllowed(f)) StartInverse(f);
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!hasF || !session.mask().canRedo());
    if (ImGui::Button("Refazer")) {
        RefreshSpectrumRect(f, session.mask().redo());
        if (f.liveIdft && LiveAllowed(f)) StartInverse(f);
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Resetar máscara")) {
        session.mask().clear();
        RefreshSpectrumFull(f);
    }
    ImGui::SameLine();
    if (ImGui::Button("Inverter")) {
        session.mask().invert();
        RefreshSpectrumFull(f);
    }

    // ---------------------------------------------------- 4. pre-definidos
    ImGui::SeparatorText("4. Filtros pré-definidos");
    {
        ImGui::SetNextItemWidth(-1);
        if (ImGui::BeginCombo("##kind", core::filterKindName(kKinds[f.presetKind]))) {
            for (int i = 0; i < 6; ++i)
                if (ImGui::Selectable(core::filterKindName(kKinds[i]), f.presetKind == i)) f.presetKind = i;
            ImGui::EndCombo();
        }
        ImGui::Text("Formato:");
        ImGui::SameLine();
        ImGui::RadioButton("Ideal", &f.presetShape, 0);
        ImGui::SameLine();
        ImGui::RadioButton("Butterworth", &f.presetShape, 1);
        ImGui::SameLine();
        ImGui::RadioButton("Gaussiano", &f.presetShape, 2);
        if (f.presetShape == 0) {
            ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.2f, 1.0f), "Filtros ideais produzem ringing.");
            Tooltip("A resposta espacial de um filtro ideal é uma função sinc: gera ondulações\n"
                    "(ringing). Butterworth de baixa ordem e Gaussiano não produzem.");
        }

        const bool notchKind = (f.presetKind >= 4);
        const bool bandKind = (f.presetKind == 2 || f.presetKind == 3);
        if (!notchKind) {
            ImGui::SetNextItemWidth(-1);
            ImGui::SliderFloat("##d0", &f.presetD0, 1.0f, 400.0f, "D0 = %.1f px", ImGuiSliderFlags_Logarithmic);
        }
        if (bandKind) {
            ImGui::SetNextItemWidth(-1);
            ImGui::SliderFloat("##w", &f.presetWidth, 1.0f, 200.0f, "largura W = %.1f px", ImGuiSliderFlags_Logarithmic);
        }
        if (f.presetShape == 1) {
            ImGui::SetNextItemWidth(-1);
            ImGui::SliderInt("##order", &f.presetOrder, 1, 10, "ordem n = %d");
        }
        if (notchKind) {
            ImGui::TextDisabled("Notches (relativos ao centro): u = linhas, v = colunas");
            int removeIdx = -1;
            for (size_t i = 0; i < f.notches.size(); ++i) {
                ImGui::PushID(static_cast<int>(i));
                float vals[3] = {static_cast<float>(f.notches[i].u), static_cast<float>(f.notches[i].v),
                                 static_cast<float>(f.notches[i].d0)};
                ImGui::SetNextItemWidth(-40);
                if (ImGui::InputFloat3("##n", vals, "%.1f")) {
                    f.notches[i].u = vals[0];
                    f.notches[i].v = vals[1];
                    f.notches[i].d0 = std::max(0.5f, vals[2]);
                }
                ImGui::SameLine();
                if (ImGui::Button("x")) removeIdx = static_cast<int>(i);
                ImGui::PopID();
            }
            if (removeIdx >= 0) f.notches.erase(f.notches.begin() + removeIdx);
            if (ImGui::Button("Adicionar notch", ImVec2(-1, 0))) f.notches.push_back(core::Notch{0.0, 20.0, 6.0});
            ImGui::TextDisabled("(u, v, D0k)  — cada notch inclui o par simétrico");
        }

        if (ImGui::Button("Aplicar filtro à máscara", ImVec2(-1, 0))) {
            session.mask().multiplyBy(core::makeFilter(session.P(), session.Q(), PresetParams(f)));
            RefreshSpectrumFull(f);
            if (f.liveIdft && LiveAllowed(f)) StartInverse(f);
        }
        Tooltip("Multiplica a máscara H atual pelo filtro (pode ser combinado com a edição manual).");
    }

    // ---------------------------------------------------- 5. resultado
    ImGui::SeparatorText("5. Imagem filtrada");
    {
        bool normChanged = false;
        ImGui::Text("Saída:");
        ImGui::SameLine();
        normChanged |= ImGui::RadioButton("Normalizar min-max", &f.normalization, 0);
        ImGui::SameLine();
        normChanged |= ImGui::RadioButton("Recortar [0,255]", &f.normalization, 1);
        if (normChanged && hasF && session.hasResult()) ShowResult(f);

        ImGui::BeginDisabled(hasF && !LiveAllowed(f));
        ImGui::Checkbox("Recalcular ao soltar o mouse", &f.liveIdft);
        ImGui::EndDisabled();
        Tooltip("Aplica a IDFT automaticamente ao terminar cada pincelada (só para tamanhos\n"
                "em que isso é rápido). Para imagens grandes use o botão Aplicar.");

        if (ImGui::Button("Aplicar (IDFT)", ImVec2(-1, 0))) StartInverse(f);
        ImGui::EndDisabled();   // !hasF  (abre em "2. Espectro")

        ImGui::BeginDisabled(!f.hasResult || busy);
        if (ImGui::Button("Usar resultado como imagem de trabalho", ImVec2(-1, 0))) {
            // A filtragem foi feita sobre a intensidade I. Em imagem colorida so o
            // plano I do HSI e substituido (matiz e saturacao preservados); em
            // cinza (R = G = B) isso equivale a trocar a imagem pelo resultado.
            if (f.result.getRows() != state.workingImage.getRows() ||
                f.result.getCols() != state.workingImage.getCols()) {
                state.statusMessage = "O resultado não tem o tamanho da imagem de trabalho atual; refaça a DFT.";
            } else {
                core::applyToPlanes(state.workingImage, core::ColorTarget::IntensityHSI,
                                    [&](core::Image& intensity) { intensity = f.result; });
                state.statusMessage = "Resultado da filtragem em frequência aplicado à imagem de trabalho.";
            }
            RefreshTexture(state);
        }
        ImGui::EndDisabled();
        if (!busy && session.hasResult()) {
            ImGui::TextDisabled("max|Im|/max|Re| residual: %.2e", session.residualImag());
            if (f.dftMs > 0) ImGui::TextDisabled("DFT: %.0f ms | IDFT: %.0f ms", f.dftMs, f.idftMs);
        }
    }

    ImGui::EndChild();
    ImGui::SameLine();

    // =================================================================
    // Direita: tres paineis lado a lado (entrada | espectro | resultado)
    // =================================================================
    ImGui::BeginGroup();
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    const float paneW = std::max(140.0f, (avail.x - 2.0f * spacing) / 3.0f);
    const float paneH = std::max(160.0f, avail.y);

    DrawPane("in", "Entrada", state.textureId, M, N, f.inPane, paneW, paneH, nullptr);
    ImGui::SameLine();

    // ---- espectro editavel
    const int P = f.dispRows, Q = f.dispCols;
    const bool canEdit = hasF && f.spectrumTexture != 0 && P > 0 && Q > 0;
    const core::Brush brush = CurrentBrush(f);
    bool strokeEnded = false;

    auto overlay = [&](ImDrawList* dl, const PaneEvents& ev, float zoom) {
        if (!canEdit || !(ev.hovered || ev.active)) return;
        const ImVec2 mouse = ImGui::GetIO().MousePos;
        if (f.tool == 0) {
            // circulo do raio (e da extensao efetiva, se maior) + o espelho conjugado
            const float r = static_cast<float>(f.radius) * zoom;
            const float ext = static_cast<float>(core::SpectrumMask::brushExtent(brush)) * zoom;
            dl->AddCircle(mouse, std::max(2.0f, r), IM_COL32(255, 80, 80, 255), 32, 1.5f);
            if (ext > r + 1.0f) dl->AddCircle(mouse, ext, IM_COL32(255, 80, 80, 110), 32, 1.0f);
            const double row = (mouse.y - ev.imgMin.y) / zoom - 0.5;
            const double col = (mouse.x - ev.imgMin.x) / zoom - 0.5;
            const ImVec2 mir(ev.imgMin.x + static_cast<float>(Q - col + 0.5) * zoom,
                             ev.imgMin.y + static_cast<float>(P - row + 0.5) * zoom);
            dl->AddCircle(mir, std::max(2.0f, r), IM_COL32(80, 160, 255, 200), 32, 1.0f);
        } else {
            dl->AddCircle(mouse, std::max(2.0f, f.notchClickD0 * zoom), IM_COL32(255, 200, 60, 255), 32, 1.5f);
        }
    };

    const PaneEvents ev = DrawPane("spec", "Espectro (editável)", f.spectrumTexture, P, Q, f.specPane,
                                   paneW, paneH, overlay);

    if (canEdit) {
        const ImVec2 mouse = ImGui::GetIO().MousePos;
        const float zoom = f.specPane.zoom;
        const ImVec2 cur(static_cast<float>((mouse.y - ev.imgMin.y) / zoom - 0.5),     // x = linha u
                         static_cast<float>((mouse.x - ev.imgMin.x) / zoom - 0.5));    // y = coluna v

        if (f.tool == 0) {
            if (ev.active) {
                std::vector<core::Rect> rects;
                if (!f.painting) {
                    f.painting = true;
                    session.mask().beginStroke();
                    rects = session.mask().paintSegment(cur.x, cur.y, cur.x, cur.y, brush);
                    f.lastPos = cur;
                } else if (cur.x != f.lastPos.x || cur.y != f.lastPos.y) {
                    rects = session.mask().paintSegment(f.lastPos.x, f.lastPos.y, cur.x, cur.y, brush);
                    f.lastPos = cur;
                }
                for (const core::Rect& r : rects) RefreshSpectrumRect(f, r);
            }
            if (f.painting && !ev.active) {
                session.mask().endStroke();
                f.painting = false;
                strokeEnded = true;
            }
        } else if (ev.clicked) {
            const std::vector<float> filter = core::makeNotchRejectAt(
                P, Q, cur.x, cur.y, f.notchClickD0, f.presetOrder, kShapes[f.notchClickShape]);
            session.mask().multiplyBy(filter);
            RefreshSpectrumFull(f);
            strokeEnded = true;
        }
    } else if (f.painting) {
        // a sessao ficou indisponivel no meio de uma pincelada (ex.: worker iniciado)
        f.painting = false;
    }

    if (strokeEnded && f.liveIdft && LiveAllowed(f)) StartInverse(f);

    ImGui::SameLine();
    DrawPane("res", "Imagem filtrada", f.hasResult ? f.resultTexture : 0,
             f.hasResult ? f.result.getRows() : 0, f.hasResult ? f.result.getCols() : 0,
             f.resPane, paneW, paneH, nullptr);
    ImGui::EndGroup();

    ImGui::End();
}

}
