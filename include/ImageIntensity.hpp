#pragma once
#include "Image.hpp"
#include <string>

namespace core {

    // Todas as transformacoes abaixo valem para imagens cinza e coloridas:
    // sao aplicadas independentemente em cada canal.

    //Limiarizacao (Thresholding)
    //s = 0 se r < limiar
    //s = 255 se r >= limiar
    void threshold(Image& img, int limiar);

    // Negativo de imagem
    //s = 255 - r
    void negative(Image& img);


    // Brilho: s = r + delta, com clamp em 0..255
    void brightness(Image& img, int delta);

    // s = c*log(1+r) - tranf log
    double computeAutoLogConstant(const Image& img);
    void logTransform(Image& img, double c, bool autoConstant);

    // s = c*r^gama - transf pot
    void gammaCorrection(Image& img, double gamma, double c);

    // contraste
    void contrastStretch(Image& img, int r1, int s1, int r2, int s2);

    // 5b. Funcoes lineares por partes: fatiamento de niveis de intensidade
    //Pixels com valor em [rMin, rMax] viram highlightValue
    //Fora do intervalo: mantem o valor original (preserveBackground = true ou vira backgroundValue (preserveBackground = false)
    void intensitySlice(Image& img, int rMin, int rMax, int highlightValue,
                         bool preserveBackground, int backgroundValue = 0);

    //Fatiamento por planos de bits
    //Produz uma imagem binaria (0 ou 255) mostrando apenas aquele plano.
    void bitPlaneSlice(Image& img, int plane);

    // Esteganografia LSB: cada amostra (cada canal de cada pixel) guarda 1 bit.
    // Os primeiros 32 bits guardam o tamanho da mensagem em bytes.
    long long steganographyCapacityBytes(const Image& img);

    bool embedMessageLSB(Image& img, const std::string& message);

    std::string extractMessageLSB(const Image& img);

}
