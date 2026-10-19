#include "ImageScale.hpp"
#include <algorithm>
#include <cmath>
#include <vector>

namespace core {

bool computeScaledSize(int rows, int cols, double sx, double sy, int& outRows, int& outCols) {
    if (rows <= 0 || cols <= 0) return false;
    if (!std::isfinite(sx) || !std::isfinite(sy) || sx <= 0.0 || sy <= 0.0) return false;

    const double wo = std::max(1.0, std::round(sx * cols));
    const double ho = std::max(1.0, std::round(sy * rows));
    if (wo * ho > static_cast<double>(kMaxOutputPixels)) return false;

    outCols = static_cast<int>(wo);
    outRows = static_cast<int>(ho);
    return true;
}

namespace {

    // Tamanho impar da caixa para uma escala efetiva s: ceil(1/s), +1 se par.
    int boxSizeForScale(double s) {
        if (s >= 1.0) return 1;
        int k = static_cast<int>(std::ceil(1.0 / s - 1e-9));
        if (k < 1) k = 1;
        if (k % 2 == 0) ++k;
        return k;
    }

    inline int clampIdx(int v, int hi) { return v < 0 ? 0 : (v > hi ? hi : v); }

}

Image boxBlur(Image& src, int kRows, int kCols) {
    const int rows = src.getRows();
    const int cols = src.getCols();
    if (rows <= 0 || cols <= 0) return src;
    if (kRows % 2 == 0) ++kRows;
    if (kCols % 2 == 0) ++kCols;
    if (kRows <= 1 && kCols <= 1) return src;

    const auto& in = src.getData();
    const int rc = std::max(0, kCols / 2);   // raio horizontal
    const int rr = std::max(0, kRows / 2);   // raio vertical

    // Passo horizontal: media deslizante em cada linha (borda replicada).
    // Resultado intermediario em float para nao arredondar duas vezes.
    std::vector<float> tmp(static_cast<size_t>(rows) * cols);
    for (int y = 0; y < rows; ++y) {
        const std::vector<int>& row = in[static_cast<size_t>(y)];
        float* t = &tmp[static_cast<size_t>(y) * cols];
        if (rc == 0) {
            for (int x = 0; x < cols; ++x) t[x] = static_cast<float>(row[static_cast<size_t>(x)]);
            continue;
        }
        long long sum = 0;
        for (int j = -rc; j <= rc; ++j) sum += row[static_cast<size_t>(clampIdx(j, cols - 1))];
        const double inv = 1.0 / (2 * rc + 1);
        for (int x = 0; x < cols; ++x) {
            t[x] = static_cast<float>(sum * inv);
            sum += row[static_cast<size_t>(clampIdx(x + rc + 1, cols - 1))];
            sum -= row[static_cast<size_t>(clampIdx(x - rc, cols - 1))];
        }
    }

    // Passo vertical: soma deslizante por coluna, percorrendo linha a linha
    // (acesso sequencial na memoria).
    Image out(rows, cols, src.getMaxIntensity(), src.getBitsPerPixel(), src.getDpi());
    auto& outData = out.getData();
    std::vector<double> colSum(static_cast<size_t>(cols), 0.0);
    for (int j = -rr; j <= rr; ++j) {
        const float* t = &tmp[static_cast<size_t>(clampIdx(j, rows - 1)) * cols];
        for (int x = 0; x < cols; ++x) colSum[static_cast<size_t>(x)] += t[x];
    }
    const double invV = 1.0 / (2 * rr + 1);
    for (int y = 0; y < rows; ++y) {
        std::vector<int>& outRow = outData[static_cast<size_t>(y)];
        for (int x = 0; x < cols; ++x) {
            outRow[static_cast<size_t>(x)] = static_cast<int>(std::lround(colSum[static_cast<size_t>(x)] * invV));
        }
        const float* tAdd = &tmp[static_cast<size_t>(clampIdx(y + rr + 1, rows - 1)) * cols];
        const float* tSub = &tmp[static_cast<size_t>(clampIdx(y - rr, rows - 1)) * cols];
        for (int x = 0; x < cols; ++x) {
            colSum[static_cast<size_t>(x)] += static_cast<double>(tAdd[x]) - static_cast<double>(tSub[x]);
        }
    }
    return out;
}

Image scaleImage(Image& src, double sx, double sy, Interpolation interp, bool antialias) {
    int outRows = 0, outCols = 0;
    if (!computeScaledSize(src.getRows(), src.getCols(), sx, sy, outRows, outCols)) {
        return Image(0, 0);
    }

    // Escala efetiva (evita deslocamentos por causa do arredondamento do tamanho).
    const double sxEf = static_cast<double>(outCols) / src.getCols();
    const double syEf = static_cast<double>(outRows) / src.getRows();

    // Suavizacao antes de reduzir (opcional). Se nada precisa ser suavizado,
    // usa a propria imagem de entrada sem copiar.
    Image smoothed(0, 0);
    Image* source = &src;
    if (antialias && (sxEf < 1.0 || syEf < 1.0)) {
        smoothed = boxBlur(src, boxSizeForScale(syEf), boxSizeForScale(sxEf));
        source = &smoothed;
    }

    // Mapeamento inverso alinhado por centro de pixel, escrito como matriz
    // afim: xs = xo/sx + (0.5/sx - 0.5), ys = yo/sy + (0.5/sy - 0.5).
    Affine2D aInv;
    aInv.at(0, 0) = 1.0 / sxEf;
    aInv.at(0, 2) = 0.5 / sxEf - 0.5;
    aInv.at(1, 1) = 1.0 / syEf;
    aInv.at(1, 2) = 0.5 / syEf - 0.5;

    // Todos os pontos de saida caem dentro do dominio; o fundo nunca e usado.
    return warpInverse(*source, aInv, outRows, outCols, interp, 0);
}

}
