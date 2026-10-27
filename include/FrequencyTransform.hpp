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

    // Progresso e cancelamento para operacoes longas (usado pela GUI com um
    // thread de trabalho). Ambos sao opcionais (nullptr).
    struct TransformControl {
        std::atomic<bool> cancel{false};
        std::atomic<float> progress{0.0f};   // 0..1
    };

    // ---------------------------------------------------------------------
    // 1-D (Eq. 4.4-6). Serve de base para a transformada 2-D separavel e para
    // os testes.
    // ---------------------------------------------------------------------

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

    // Transformada direta 2-D no proprio lugar, pela FFT radix-2 propria
    // (separabilidade: linhas e depois colunas). Retorna false se cancelada
    // (control->cancel) ou se linhas/colunas nao forem potencia de 2; nesses
    // casos 'm' pode ficar em estado indeterminado.
    bool forwardTransform2D(ComplexMatrix& m, TransformControl* control = nullptr);

    // Transformada inversa 2-D no proprio lugar, usando a mesma FFT direta
    // (Secao 4.11.2):  f = conj( FFT{ conj(F) } ) / (M N).
    bool inverseTransform2D(ComplexMatrix& m, TransformControl* control = nullptr);

}
