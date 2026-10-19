#pragma once
#include "Image.hpp"
#include "ImageWarp.hpp"

namespace core {

    // Sinal da rotacao: angulo POSITIVO = sentido ANTI-HORARIO visual (o que o
    // usuario espera). Como o eixo y aponta para baixo, isso corresponde a
    // R = [[cos, sin], [-sin, cos]]. Para inverter o sentido, basta trocar
    // esta constante para -1.0 (e so aqui).
    constexpr double kCounterClockwiseSign = 1.0;

    // Tela de saida da rotacao.
    //  KeepSize: Ho = H, Wo = W, deslocamento o = (0, 0). Parte da imagem pode
    //            ser cortada; aqui o pivo muda o resultado.
    //  Expand:   a saida e o retangulo envolvente da imagem rotacionada; o
    //            resultado nao depende do pivo (so a posicao e recentralizada).
    enum class RotationCanvas { KeepSize, Expand };

    struct RotationLayout {
        int outRows = 0;
        int outCols = 0;
        double offsetX = 0.0;   // o = (offsetX, offsetY)
        double offsetY = 0.0;
    };

    // Pivo padrao: centro da imagem, ((W-1)/2, (H-1)/2).
    void defaultPivot(int rows, int cols, double& px, double& py);

    // Matriz direta (entrada -> saida) da rotacao em torno do pivo, sem o
    // deslocamento da tela:  p' = R (p - c) + c.
    Affine2D rotationAboutPivot(double angleDegrees, double px, double py);

    // Calcula tamanho e deslocamento da tela de saida (Secao 3.6).
    // Expand: transforma os 4 cantos do retangulo continuo
    // (-0.5,-0.5), (W-0.5,-0.5), (W-0.5,H-0.5), (-0.5,H-0.5), pega o retangulo
    // envolvente [xmin,xmax] x [ymin,ymax] e usa
    //   Wo = ceil(xmax - xmin - eps), Ho = ceil(ymax - ymin - eps),
    //   o  = (xmin + 0.5, ymin + 0.5).
    // Retorna false se os dados forem invalidos ou a saida passar de
    // kMaxOutputPixels (100 megapixels).
    bool computeRotationLayout(int rows, int cols, double angleDegrees, double px, double py,
                               RotationCanvas canvas, RotationLayout& layout);

    // Rotaciona por mapeamento inverso:  p = R^T (p' + o - c) + c.
    // background: valor (0-255) dos pixels que ficam fora da imagem original.
    // Retorna imagem vazia (0 x 0) se os parametros forem invalidos.
    Image rotateImage(Image& src, double angleDegrees, double px, double py,
                      Interpolation interp, RotationCanvas canvas, int background);

}
