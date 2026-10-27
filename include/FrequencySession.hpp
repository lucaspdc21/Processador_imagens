#pragma once
#include "Image.hpp"
#include "FrequencyMask.hpp"
#include "FrequencyTransform.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace core {

    // Limite de seguranca: P x Q maximo (complex128 = 16 bytes por ponto).
    constexpr long long kMaxTransformPixels = 4096LL * 4096LL;

    struct TransformSize {
        int P = 0;
        int Q = 0;
    };

    // Tamanho P x Q da transformada, SEM preenchimento (zero-padding de 2M x 2N):
    //  a FFT radix-2 exige potencias de 2, entao P e Q sao apenas a menor
    //  potencia de 2 >= M e >= N (minimo 2, para que (-1)^(x+y) centralize).
    //  Ex.: 720 x 1280 -> 1024 x 2048 (e nao 2048 x 4096, como seria com P >= 2M-1).
    TransformSize computeTransformSize(int M, int N);

    enum class DisplayMode {
        FilteredSpectrum,   // log(1 + |H F|)  (o que o usuario riscou)
        OriginalSpectrum    // log(1 + |F|)
    };

    enum class OutputNormalization {
        MinMax,             // leva [min, max] a [0, 255] (padrao)
        Clip                // recorta em [0, 255]
    };

    // Roteiro completo de filtragem (Secao 4.7.3). A DFT direta e calculada
    // UMA vez e guardada em cache (complex128); a cada "Aplicar" so se
    // recalcula G = H F e a inversa.
    class FrequencySession {
    public:
        // Passos 1-3: copia a imagem direto para a matriz de entrada P x Q da
        // FFT (sem preenchimento 2M x 2N; P e Q so sobem ate a potencia de 2
        // exigida pela radix-2), multiplica por (-1)^(x+y) (centraliza) e
        // calcula F. Pesado: a GUI chama em outro thread. Retorna false se
        // cancelado ou se os parametros forem invalidos (ver lastError()).
        bool compute(Image& img, TransformControl* control = nullptr);

        // Passos 5-7: G = H F, IFFT, parte real e (-1)^(x+y). O resultado bruto
        // (M x N, double) fica em resultRaw(): so a regiao da imagem e lida da
        // matriz da IFFT (nao ha recorte de uma matriz expandida 2M x 2N).
        bool filter(TransformControl* control = nullptr);

        bool hasTransform() const { return hasF_; }
        bool hasResult() const { return hasResult_; }
        const std::string& lastError() const { return error_; }

        int M() const { return M_; }
        int N() const { return N_; }
        int P() const { return P_; }
        int Q() const { return Q_; }
        const ComplexMatrix& spectrum() const { return F_; }

        // S_max = max log(1 + |F|) do espectro original (normaliza a exibicao).
        double logMax() const { return logMax_; }

        SpectrumMask& mask() { return mask_; }
        const SpectrumMask& mask() const { return mask_; }

        // ---- exibicao do espectro (RGBA 8 bits, P x Q) -----------------
        void setDisplayMode(DisplayMode mode);
        DisplayMode displayMode() const { return mode_; }
        // Recalcula a imagem de exibicao (tudo, ou so uma regiao).
        void renderAll();
        void renderRect(const Rect& r);
        const std::vector<uint8_t>& displayRGBA() const { return rgba_; }

        // ---- resultado ------------------------------------------------
        const std::vector<double>& resultRaw() const { return result_; }
        // max|Im| / max|Re| do resultado antes de descartar a parte imaginaria
        // (deve ser ~0 por causa da simetria conjugada da mascara).
        double residualImag() const { return residualImag_; }
        // Converte o resultado bruto (M x N) para imagem 8 bits.
        Image resultImage(OutputNormalization norm);

    private:
        void renderPixelRow(int r, int c0, int c1);

        bool hasF_ = false;
        bool hasResult_ = false;
        int M_ = 0, N_ = 0, P_ = 0, Q_ = 0;
        int maxIntensity_ = 255, bits_ = 8;
        double dpi_ = 72.0;
        ComplexMatrix F_;
        double logMax_ = 1.0;
        SpectrumMask mask_;
        DisplayMode mode_ = DisplayMode::FilteredSpectrum;
        std::vector<uint8_t> rgba_;
        std::vector<double> result_;
        double residualImag_ = 0.0;
        std::string error_;
    };

}
