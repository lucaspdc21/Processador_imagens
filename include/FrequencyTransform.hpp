#pragma once
#include <atomic>
#include <complex>
#include <functional>
#include <vector>

namespace core {

    using Complex = std::complex<double>;

    // Matriz complexa (rows x cols), armazenada linha a linha.
    // Convencao do Cap. 4: f(x, y) com x = linha (0..M-1) e y = coluna (0..N-1);
    // F(u, v) com u = linha (0..M-1) e v = coluna (0..N-1).
    struct ComplexMatrix {
        int rows = 0;
        int cols = 0;
        std::vector<Complex> data;

        ComplexMatrix() = default;
        ComplexMatrix(int r, int c) : rows(r), cols(c), data(static_cast<size_t>(r) * static_cast<size_t>(c)) {}

        Complex& at(int r, int c) { return data[static_cast<size_t>(r) * cols + c]; }
        const Complex& at(int r, int c) const { return data[static_cast<size_t>(r) * cols + c]; }
    };

    // Algoritmos selecionaveis (Secao 4.11 do plano).
    enum class TransformAlgorithm {
        NaiveDFT,      // DFT propria "ingenua" separavel: O(MN(M+N))
        RadixFFT,      // FFT propria radix-2 (duplicacao sucessiva): O(MN log(MN))
        LibraryFFT     // FFT de biblioteca (pocketfft), quando disponivel
    };

    const char* algorithmName(TransformAlgorithm algo);

    // true se o programa foi compilado com a biblioteca de FFT (pocketfft).
    bool libraryFFTAvailable();

    // Progresso e cancelamento para operacoes longas (usado pela GUI com um
    // thread de trabalho). Ambos sao opcionais (nullptr).
    struct TransformControl {
        std::atomic<bool> cancel{false};
        std::atomic<float> progress{0.0f};   // 0..1
    };

    // ---------------------------------------------------------------------
    // 1-D (Eq. 4.4-6 e 4.4-7). Servem de referencia e para os testes.
    // ---------------------------------------------------------------------

    // DFT 1-D ingenua: F(u) = sum_x f(x) e^{-j 2 pi u x / M}. Qualquer tamanho.
    std::vector<Complex> dft1d(const std::vector<Complex>& f);

    // IDFT 1-D ingenua: f(x) = (1/M) sum_u F(u) e^{+j 2 pi u x / M}.
    std::vector<Complex> idft1d(const std::vector<Complex>& F);

    // FFT 1-D radix-2 propria (Secao 4.11.3). Exige tamanho potencia de 2;
    // para outros tamanhos retorna vetor vazio.
    std::vector<Complex> fft1d(const std::vector<Complex>& f);

    bool isPowerOfTwo(int n);
    int nextPowerOfTwo(int n);

    // Executa fn(i) para i = 0..n-1 em varios threads (agendamento dinamico).
    void parallelForEach(int n, const std::function<void(int)>& fn);

    // ---------------------------------------------------------------------
    // 2-D. A transformada direta NAO e normalizada (Eq. 4.5-15); a constante
    // 1/MN fica na inversa (Eq. 4.5-16).
    // ---------------------------------------------------------------------

    // Transformada direta 2-D no proprio lugar. Retorna false se cancelada
    // (control->cancel) ou se o algoritmo nao puder tratar o tamanho
    // (RadixFFT exige linhas e colunas potencia de 2; LibraryFFT exige a
    // biblioteca compilada). Nesses casos 'm' pode ficar em estado
    // indeterminado.
    bool forwardTransform2D(ComplexMatrix& m, TransformAlgorithm algo, TransformControl* control = nullptr);

    // Transformada inversa 2-D no proprio lugar, usando UM unico algoritmo
    // direto (Secao 4.11.2):  f = conj( DFT{ conj(F) } ) / (M N).
    bool inverseTransform2D(ComplexMatrix& m, TransformAlgorithm algo, TransformControl* control = nullptr);

    // Estimativa de tempo (segundos) da DFT ingenua separavel P x Q, com base
    // numa calibracao rapida da maquina (feita uma vez). A GUI usa para avisar.
    double estimateNaiveSeconds(int P, int Q);

}
