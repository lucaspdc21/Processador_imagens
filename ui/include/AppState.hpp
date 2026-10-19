#pragma once
#include <string>
#include <vector>
#include "Image.hpp"
#include "ColorImage.hpp"
#include "ImageColor.hpp"
#include "ImageResample.hpp"
#include "ImageHistogram.hpp"
#include "ImageConvolution.hpp"

namespace ui {

    // Guarda tudo que os tres paineis precisam compartilhar
    struct AppState {
        core::ColorImage originalImage;   // imagem RGB (cinza = R, G e B iguais)
        core::ColorImage workingImage;
        bool hasImage = false;

        // Incrementado a cada RefreshTexture (imagem de trabalho mudou). As
        // janelas auxiliares usam para saber quando descartar previas antigas.
        int imageRevision = 0;

        unsigned int textureId = 0; // id de textura OpenGL 

        int bitsPerPixel = 8;       // k selecionado na interface
        float dpiValue = 72.0f;     // dpi selecionado na interface
        int resamplePercent = 100;  // % do tamanho original para reamostragem

        core::resampleType interpolationMethod = core::resampleType::Nearest;
        std::string statusMessage = "Nenhuma imagem carregada.";

        // parâmetros das tranf de intensidade
        int thresholdValue = 128;       

        float brightnessFactor = 1.5f;     // k em s = k * r

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

        // Histograma (cache: recalculado quando a imagem muda, ver RefreshTexture)
        core::ColorHistograms histogramWorking;    // R, G, B e I
        core::ColorHistograms histogramOriginal;
        bool histogramDirty = true;
        bool histogramNormalized = false;   // false: h(rk) = nk | true: p(rk) = nk/MN

        // Janela de escala e rotacao (operacoes espaciais)
        bool showSpatialWindow = false;

        // Janela de filtragem no dominio da frequencia (DFT/FFT, espectro editavel)
        bool showFrequencyWindow = false;

        // Filtro por convolucao (kernel generico quadrado de tamanho impar)
        bool showConvolutionWindow = false;
        int kernelSize = 3;
        std::vector<float> kernelValues = {0.0f, 0.0f, 0.0f,
                                           0.0f, 1.0f, 0.0f,
                                           0.0f, 0.0f, 0.0f};   // linha a linha (identidade)
        bool kernelDivideBySum = false;
        int filterOperation = 0;    // 0: convolucao | 1: correlacao
        int filterBorder = 0;       // 0: preencher com zeros | 1: replicar a borda
        int filterResultMode = 0;   // 0: limitar a [0,255] | 1: manter valores (normaliza so na exibicao)

        // Filtros espaciais: suavizacao, mediana, Laplaciano, high-boost, Sobel
        int smoothType = 0;              // 0: media simples | 1: media ponderada (1-2-1) | 2: gaussiano
        int smoothSize = 3;
        float gaussianSigma = 1.0f;
        int medianSize = 3;
        int laplacianChoice = 0;         // 0: vizinhos-4 (centro -4) | 1: vizinhos-8 (centro -8) | 2 e 3: centro positivo
        float highBoostK = 2.0f;
        bool gradientApproximate = false;   // true: |gx| + |gy| | false: raiz(gx^2 + gy^2)
        int spatialBorder = 1;           // 0: zeros | 1: replicar a borda
        bool normalizeNextDisplay = false;  // pede a RefreshTexture para normalizar so a exibicao

        core::BorderMode spatialBorderMode() const {
            return spatialBorder == 1 ? core::BorderMode::Replicate : core::BorderMode::Zero;
        }

        // ---- Imagens coloridas ----
        // Onde aplicar as transformacoes de intensidade e os filtros:
        // 0: em R, G e B separadamente | 1: so no plano I do HSI (cores preservadas)
        int colorTargetChoice = 0;
        core::ColorTarget colorTarget() const {
            return colorTargetChoice == 1 ? core::ColorTarget::IntensityHSI : core::ColorTarget::ChannelsRGB;
        }

        // Visualizacao (nao altera a imagem): 0 = cor | 1 = R | 2 = G | 3 = B | 4 = H | 5 = S | 6 = I
        int viewChoice = 0;
        core::ColorView colorView() const { return static_cast<core::ColorView>(viewChoice); }

        // Escala de cinza (media ponderada). Padrao ITU-R BT.601.
        float grayWeightR = 0.299f;
        float grayWeightG = 0.587f;
        float grayWeightB = 0.114f;

        // Sepia
        int sepiaMode = 0;               // 0: matriz classica | 1: via HSI
        float sepiaHue = 35.0f;          // graus (modo HSI)
        float sepiaSaturation = 0.30f;   // (modo HSI)

        // Chroma-key
        core::ColorImage chromaBackground;
        bool hasChromaBackground = false;
        std::string chromaBackgroundName;
        float chromaKeyColor[3] = {0.0f, 1.0f, 0.0f};   // cor-chave a (RGB em [0, 1])
        float chromaTolerance = 40.0f;                  // D0 em % da distancia maxima
        float chromaFeather = 0.0f;                     // borda suave, em %
        int chromaMode = 0;                             // 0: esfera (euclidiana) | 1: cubo
        bool chromaPicking = false;                     // conta-gotas ativo (clique na imagem)
        int chromaPickRadius = 2;                       // media numa janela (2r+1)^2

        // Ajuste de matiz, saturacao e brilho (HSI)
        float hsiDeltaHue = 0.0f;        // graus
        float hsiSaturationFactor = 1.0f;
        float hsiIntensityFactor = 1.0f;

        // Ajuste de canal C/R, M/G, Y/B (niveis, -255..+255)
        int channelDeltaR = 0;
        int channelDeltaG = 0;
        int channelDeltaB = 0;

        // Equalizacao de histograma colorida: ganho de saturacao depois de equalizar
        float equalizeSaturationGain = 1.0f;

        // Bordas em imagem colorida: gradiente vetorial de Di Zenzo
        bool gradientDiZenzo = false;

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
        void applyBrightness();
        void applyThreshold();
        void applyLogTransform();
        void applyGammaCorrection();
        void applyContrastStretch();
        void applyIntensitySlice();
        void applyBitPlaneSlice();
        void applyHistogramEqualization();

        // Imagens coloridas
        void applyGrayAverage();
        void applyGrayWeighted();
        void applySepia();
        bool loadChromaBackground(std::string& path);
        void applyChromaKey();
        core::ChromaKeyParams chromaKeyParams() const;
        // Conta-gotas: usa a media da janela em torno do pixel (row, col) como cor-chave
        void pickChromaColor(int row, int col);
        void applyHsiAdjust();
        void applyChannelAdjust();
        // Troca a imagem de trabalho pelo plano que esta sendo visualizado (R, G, B, H, S ou I)
        void applyViewToImage();

        // Recalcula os histogramas (atual e original) se a imagem mudou.
        void updateHistograms();

        // Muda o tamanho do kernel (impar), preservando os coeficientes centrais.
        void setKernelSize(int newSize);

        // Aplica o kernel editado na imagem de trabalho.
        void applyConvolution();

        // Kernel de suavizacao escolhido (media simples, ponderada ou gaussiano).
        core::Kernel smoothingKernel() const;

        void applySmoothing();
        void applyMedian();
        void showLaplacian();            // Laplaciano puro (exibido normalizado)
        void applyLaplacianSharpen();    // g = f + c * laplaciano
        void applyHighBoost();           // g = f + k * (f - f_borrada)
        void applySobelX();
        void applySobelY();
        void applyGradientMagnitude();

        // Esteganografia LSB
        long long steganographyCapacityBytes();
        bool embedSteganoMessage();
        void extractSteganoMessage();
    };

}
