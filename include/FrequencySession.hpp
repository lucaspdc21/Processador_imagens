#pragma once
#include "Image.hpp"
#include "FrequencyMask.hpp"
#include "FrequencyTransform.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace core {

    // Limite de seguranca: P x Q maximo (complex128 = 16 bytes por ponto).
    constexpr long long kMaxPaddedPixels = 4096LL * 4096LL;

    struct PaddedSize {
        int P = 0;
        int Q = 0;
    };

    // Tamanho P x Q da transformada (Secao 4.6):
    //  Com preenchimento: P >= 2M-1 e Q >= 2N-1 (evita wraparound).
    //     DFT ingenua / biblioteca: P = 2M, Q = 2N (pares).
    //     FFT radix-2 propria:      proxima potencia de 2 >= 2M-1 (idem Q).
    //  Sem preenchimento: P = M, Q = N (mais rapido, com wraparound), apenas
    //     arredondando para PAR (necessario para centralizar por (-1)^(x+y));
    //     na FFT radix-2, para a potencia de 2 seguinte.
    PaddedSize computePaddedSize(int M, int N, TransformAlgorithm algo, bool usePadding);

    enum class DisplayMode {
        FilteredSpectrum,   // log(1 + |H F|)  (o que o usuario riscou)
        OriginalSpectrum,   // log(1 + |F|)
        MaskView,           // a propria mascara H
        Phase               // fase de F, normalizada (so informativa)
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
        // Passos 1-3: preenche com zeros (P x Q, imagem no canto superior
        // esquerdo), multiplica por (-1)^(x+y) (centraliza) e calcula F.
        // Pesado: a GUI chama em outro thread. Retorna false se cancelado ou
        // se os parametros forem invalidos (mensagem em lastError()).
        bool compute(Image& img, TransformAlgorithm algo, bool usePadding, TransformControl* control = nullptr);

        // Passos 5-7: G = H F, IDFT, parte real, (-1)^(x+y), recorte [0:M, 0:N].
        // O resultado bruto fica em resultRaw() (double, M x N).
        bool filter(TransformAlgorithm algo, TransformControl* control = nullptr);

        bool hasTransform() const { return hasF_; }
        bool hasResult() const { return hasResult_; }
        const std::string& lastError() const { return error_; }

        int M() const { return M_; }
        int N() const { return N_; }
        int P() const { return P_; }
        int Q() const { return Q_; }
        TransformAlgorithm algorithm() const { return algo_; }
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
        TransformAlgorithm algo_ = TransformAlgorithm::RadixFFT;
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
