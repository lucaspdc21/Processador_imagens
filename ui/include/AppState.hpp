#pragma once
#include <string>
#include <vector>
#include "Image.hpp"
#include "ImageResample.hpp"

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

        core::resampleType interpolationMethod = core::resampleType::Nearest;
        std::string statusMessage = "Nenhuma imagem carregada.";

        // parâmetros das tranf de intensidade
        int thresholdValue = 128;       

        bool logAutoConstant = true;       
        float logConstant = 1.0f;          

        float gammaValue = 1.0f;           
        float gammaConstant = 1.0f;        

        int stretchR1 = 70;                
        int stretchS1 = 0;
        int stretchR2 = 180;               
        int stretchS2 = 255;

        int sliceMin = 100;                
        int sliceMax = 180;
        int sliceHighlight = 255;          
        bool slicePreserveBackground = true; 

        int bitPlaneSelected = 8;          

        char steganoMessageBuffer[256] = ""; 
        std::string steganoExtractedMessage; 

        //Janela de comparação de imagens
        bool showCompareWindow = false;
        bool compareTexturesBuilt = false;
        std::vector<unsigned int> compareTextureIds;
        std::vector<std::string> compareLabels;

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

        // Tranformações
        void applyNegative();
        void applyThreshold();
        void applyLogTransform();
        void applyGammaCorrection();
        void applyContrastStretch();
        void applyIntensitySlice();
        void applyBitPlaneSlice();

        // Esteganografia LSB
        long long steganographyCapacityBytes();
        bool embedSteganoMessage();
        void extractSteganoMessage();
    };

}
