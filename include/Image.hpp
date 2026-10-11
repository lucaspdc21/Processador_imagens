#pragma once
#include <cstddef>
#include <vector>

namespace core {

    // Valor maximo de intensidade de um canal (imagens de 8 bits).
    // A quantizacao altera apenas o metadado bitsPerPixel; os valores continuam em 0..255.
    inline constexpr int kMaxIntensity = 255;

    // Imagem com 1 canal (cinza) ou 3 canais (RGB).
    // Os dados ficam em um unico vetor contiguo, linha a linha, com os canais
    // de cada pixel lado a lado: indice = (row * cols + col) * channels + ch.
    // Os valores sao float para nao perder precisao nos passos intermediarios;
    // o clamp para 0..255 e feito pelos algoritmos ao gravar o resultado e na exibicao.
    class Image {
        private:
            int rows = 0;
            int cols = 0;
            int maxIntensity = kMaxIntensity;
            int bitsPerPixel = 8;
            double dpi = 72.0;
            int channels = 1;
            std::vector<float> data;

        public:
            Image() = default;
            // channels vem por ultimo para manter compativeis as chamadas antigas
            // Image(rows, cols, maxIntensity, bitsPerPixel, dpi).
            Image(int rows, int cols, int maxIntensity = kMaxIntensity, int bitsPerPixel = 8,
                  double dpi = 72.0, int channels = 1);

            // Acesso ao canal ch do pixel (row, col). Sem verificacao de limites.
            float& at(int row, int col, int ch = 0) {
                return data[(static_cast<size_t>(row) * cols + col) * channels + ch];
            }
            float at(int row, int col, int ch = 0) const {
                return data[(static_cast<size_t>(row) * cols + col) * channels + ch];
            }

            void fill(float value);
            // Limita todas as amostras a [0, maxIntensity].
            void clampToRange();

            int getRows() const { return rows; }
            int getCols() const { return cols; }
            int getChannels() const { return channels; }
            bool empty() const { return data.empty(); }

            // Todas as amostras (rows * cols * channels), para percorrer a imagem inteira.
            std::vector<float>& samples() { return data; }
            const std::vector<float>& samples() const { return data; }
            size_t sampleCount() const { return data.size(); }

            int getMaxIntensity() const { return maxIntensity; }
            int getBitsPerPixel() const { return bitsPerPixel; }
            void setBitsPerPixel(int k) { bitsPerPixel = k; }

            double getDpi() const { return dpi; }
            void setDpi(double newDpi) { dpi = newDpi; }
    };
}
