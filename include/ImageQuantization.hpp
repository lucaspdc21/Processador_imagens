#pragma once
#include "Image.hpp"

namespace core {

    // Numero de niveis de intensidade L = 2^k
    long long levelsForBits(int k);

    // Requantiza a imagem para k bits por pixel 
    void quantize(Image& img, int k);

}
