#pragma once
#include <string>
#include <vector>
#include "Image.hpp"
#include "ImageMetrics.hpp"
#include "ImageResample.hpp"
#include "ImageStats.hpp"

namespace ui {

    // Guarda tudo que os tres paineis precisam compartilhar
    struct AppState {
        core::Image originalImage;
        core::Image workingImage;
        bool hasImage = false;

        unsigned int textureId = 0; // id de textura OpenGL 

        int bitsPerPixel = 8;       // k selecionado na interface
        float dpiValue = 72.0f;     // dpi selecionado na interface
        int resamplePercent = 100;  // % do tamanho atual da imagem para reamostragem

        core::resampleType interpolationMethod = core::resampleType::Nearest;
        std::string statusMessage = "Nenhuma imagem carregada.";

        // Exibe a imagem com normalizacao min-max (so na textura, nao altera os dados).
        // Util para resultados com valores negativos ou acima de 255 (Laplaciano, Sobel...).
        bool displayNormalized = false;

        // parâmetros das tranf de intensidade
        int brightnessValue = 0;
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

        // Historico para desfazer: copias de workingImage tiradas antes de cada operacao.
        std::vector<core::Image> history;

        // Estatisticas exibidas no painel direito. So sao recalculadas quando a imagem muda.
        bool statsDirty = true;
        core::ImageStats cachedStats;
        core::StorageMetrics cachedStorage;

        // Carrega uma imagem do disco e reseta os parametros da interface.
        bool loadFromPath(const std::string& path);

        // Salva a imagem atualmente em edicao (workingImage). O formato (PNG, BMP ou JPG)
        // vem da extensao do arquivo; sem extensao valida, acrescenta .png.
        bool saveToPath(const std::string& path);

        // Desfazer
        bool canUndo() const { return !history.empty(); }
        void pushHistory();   // chamada no inicio de cada operacao que altera a imagem
        void undo();

        // Recalcula cachedStats e cachedStorage se a imagem mudou desde a ultima vez.
        void updateStatsIfNeeded();

        // Aplica a quantizacao (posterizacao) com o valor de bitsPerPixel atual.
        void applyQuantization();

        // Aplica a reamostragem espacial com o valor de resamplePercent atual.
        void applyResample();

        // Atualiza o dpi tanto da imagem de trabalho quanto da original.
        void applyDpi(float newDpi);

        // Volta workingImage para o estado original carregado do arquivo.
        void restoreOriginal();

        // Tranformações
        void applyBrightness();
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
