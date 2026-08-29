#pragma once
#include <string>
#include "Image.hpp"

namespace ui {

    // Guarda tudo que os tres paineis precisam compartilhar
    struct AppState {
        core::Image originalImage;
        core::Image workingImage;
        bool hasImage = false;

        unsigned int textureId = 0; // id de textura OpenGL 

        int bitsPerPixel = 8;       // k selecionado na interface
        float dpiValue = 72.0f;     // dpi selecionado na interface
        int resamplePercent = 100;  // % do tamanho original para reamostragem

        std::string statusMessage = "Nenhuma imagem carregada.";

        // Carrega uma imagem do disco e reseta os parametros da interface.
        bool loadFromPath(std::string& path);

        // Salva a imagem atualmente em edicao (workingImage) como PNG.
        bool saveToPath(std::string& path);

        // Aplica a quantizacao (posterizacao) com o valor de bitsPerPixel atual.
        void applyQuantization();

        // Aplica a reamostragem espacial com o valor de resamplePercent atual.
        void applyResample();

        // Atualiza o dpi tanto da imagem de trabalho quanto da original.
        void applyDpi(float newDpi);

        // Volta workingImage para o estado original carregado do arquivo.
        void restoreOriginal();
    };

}
