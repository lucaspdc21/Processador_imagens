#pragma once
#include "Image.hpp"

namespace core {

    // Imagem colorida RGB: tres planos, cada um uma core::Image (matriz de
    // inteiros 0..255). Assim todo processamento ja escrito para escala de
    // cinza pode ser aplicado plano a plano (processamento por canal, Cap. 6.4).
    //
    // Uma imagem em escala de cinza e uma ColorImage com R = G = B.
    class ColorImage {
        private:
            Image planes[3];   // 0 = R, 1 = G, 2 = B

        public:
            static constexpr int kChannels = 3;

            ColorImage() : planes{Image(0, 0), Image(0, 0), Image(0, 0)} {}
            ColorImage(int rows, int cols, int maxIntensity = 255, int bitsPerPixel = 8, double dpi = 72.0)
                : planes{Image(rows, cols, maxIntensity, bitsPerPixel, dpi),
                         Image(rows, cols, maxIntensity, bitsPerPixel, dpi),
                         Image(rows, cols, maxIntensity, bitsPerPixel, dpi)} {}

            // Replica uma imagem de 1 canal nos tres planos (R = G = B).
            explicit ColorImage(const Image& gray) : planes{gray, gray, gray} {}

            Image& channel(int i) { return planes[i]; }
            Image& r() { return planes[0]; }
            Image& g() { return planes[1]; }
            Image& b() { return planes[2]; }

            int getRows() { return planes[0].getRows(); }
            int getCols() { return planes[0].getCols(); }
            int getMaxIntensity() { return planes[0].getMaxIntensity(); }
            int getBitsPerPixel() { return planes[0].getBitsPerPixel(); }
            double getDpi() { return planes[0].getDpi(); }

            // Os metadados valem para a imagem inteira, entao vao para os 3 planos
            void setBitsPerPixel(int k) { for (auto& p : planes) p.setBitsPerPixel(k); }
            void setDpi(double dpi) { for (auto& p : planes) p.setDpi(dpi); }

            void clampToRange() { for (auto& p : planes) p.clampToRange(); }

            // true se R = G = B em todos os pixels
            bool isGray();
    };

    // Plano I = round((R + G + B) / 3), o mesmo I do espaco HSI, em 8 bits.
    Image intensityPlane(ColorImage& img);

    // Normalizacao min-max para exibicao com os 3 planos juntos (o mesmo
    // minimo e maximo para R, G e B, para nao alterar as cores). Retorna uma
    // copia; a imagem original nao e alterada.
    ColorImage normalizeForDisplay(ColorImage& img);

}
