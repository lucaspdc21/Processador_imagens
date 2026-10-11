#pragma once
#include "Image.hpp"
#include <string>

namespace core {

// Abre BMP, JPG, PNG ou TGA. Imagens em cinza ficam com 1 canal e coloridas com 3 (RGB).
// O canal alfa, se existir, e descartado. Um BMP em que todo pixel tem R = G = B
// (como o gerado ao salvar uma imagem cinza) e aberto com 1 canal.
bool loadImage(const std::string& path, Image& outImage);

// Grava a imagem no formato indicado pela extensao do arquivo (.png, .bmp, .jpg ou .jpeg).
// Extensao desconhecida ou ausente grava PNG. JPG usa compressao com perdas.
bool saveImage(const std::string& path, const Image& image);

// Retorna true se a extensao do caminho for uma das aceitas por saveImage.
bool hasSupportedSaveExtension(const std::string& path);

// Retorna true se o caminho termina em .jpg ou .jpeg.
bool isJpegPath(const std::string& path);

}
