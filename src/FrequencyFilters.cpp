#include "FrequencyFilters.hpp"
#include <algorithm>
#include <cmath>

namespace core {

const char* filterKindName(FilterKind kind) {
    switch (kind) {
        case FilterKind::LowPass:     return "Passa-baixa";
        case FilterKind::HighPass:    return "Passa-alta";
        case FilterKind::BandReject:  return "Rejeita-banda";
        case FilterKind::BandPass:    return "Passa-banda";
        case FilterKind::NotchReject: return "Notch (rejeita)";
        case FilterKind::NotchPass:   return "Notch (passa)";
    }
    return "?";
}

const char* filterShapeName(FilterShape shape) {
    switch (shape) {
        case FilterShape::Ideal:       return "Ideal";
        case FilterShape::Butterworth: return "Butterworth";
        case FilterShape::Gaussian:    return "Gaussiano";
    }
    return "?";
}

namespace {

    // Passa-baixa em funcao da distancia D ao centro.
    //   Ideal:        H = 1 se D <= D0, senao 0
    //   Butterworth:  H = 1 / (1 + (D/D0)^(2n))
    //   Gaussiano:    H = exp(-D^2 / (2 D0^2))
    double lowPassValue(double D, FilterShape shape, double d0, int n) {
        d0 = std::max(d0, 1e-9);
        switch (shape) {
            case FilterShape::Ideal:
                return D <= d0 ? 1.0 : 0.0;
            case FilterShape::Butterworth:
                return 1.0 / (1.0 + std::pow(D / d0, 2.0 * n));
            case FilterShape::Gaussian:
                return std::exp(-(D * D) / (2.0 * d0 * d0));
        }
        return 1.0;
    }

    // Rejeita-banda em funcao de D (centro D0, largura W).
    //   Ideal:        H = 0 se D0 - W/2 <= D <= D0 + W/2, senao 1
    //   Butterworth:  H = 1 / (1 + [ D W / (D^2 - D0^2) ]^(2n))
    //   Gaussiano:    H = 1 - exp( -[ (D^2 - D0^2) / (D W) ]^2 )
    double bandRejectValue(double D, FilterShape shape, double d0, double w, int n) {
        w = std::max(w, 1e-9);
        switch (shape) {
            case FilterShape::Ideal:
                return (D >= d0 - w / 2.0 && D <= d0 + w / 2.0) ? 0.0 : 1.0;
            case FilterShape::Butterworth: {
                const double den = D * D - d0 * d0;
                if (std::fabs(den) < 1e-12) return 0.0;                 // D = D0: rejeita totalmente
                return 1.0 / (1.0 + std::pow(D * w / den, 2.0 * n));
            }
            case FilterShape::Gaussian: {
                if (D < 1e-12) return 1.0;                              // limite D -> 0: H -> 1
                const double r = (D * D - d0 * d0) / (D * w);
                return 1.0 - std::exp(-r * r);
            }
        }
        return 1.0;
    }

    // Termo de um notch (rejeita) para a distancia Dk ao centro do notch.
    //   Butterworth (Eq. 4.10-5): 1 / (1 + (D0k / Dk)^(2n))
    //   Ideal:                    0 se Dk <= D0k, senao 1
    //   Gaussiano:                1 - exp(-Dk^2 / (2 D0k^2))
    double notchTerm(double Dk, FilterShape shape, double d0k, int n) {
        d0k = std::max(d0k, 1e-9);
        switch (shape) {
            case FilterShape::Ideal:
                return Dk <= d0k ? 0.0 : 1.0;
            case FilterShape::Butterworth:
                if (Dk < 1e-12) return 0.0;                             // centro do notch
                return 1.0 / (1.0 + std::pow(d0k / Dk, 2.0 * n));
            case FilterShape::Gaussian:
                return 1.0 - std::exp(-(Dk * Dk) / (2.0 * d0k * d0k));
        }
        return 1.0;
    }

    // H_NR(u,v) = prod_k [ term(D_k) * term(D_-k) ]  (Eq. 4.10-5)
    std::vector<float> notchReject(int P, int Q, const std::vector<Notch>& notches,
                                   FilterShape shape, int n) {
        std::vector<float> h(static_cast<size_t>(P) * static_cast<size_t>(Q), 1.0f);
        const double cu = P / 2.0, cv = Q / 2.0;
        for (int u = 0; u < P; ++u) {
            for (int v = 0; v < Q; ++v) {
                double value = 1.0;
                for (const Notch& k : notches) {
                    const double du1 = u - cu - k.u, dv1 = v - cv - k.v;     // D_k
                    const double du2 = u - cu + k.u, dv2 = v - cv + k.v;     // D_-k
                    value *= notchTerm(std::sqrt(du1 * du1 + dv1 * dv1), shape, k.d0, n);
                    value *= notchTerm(std::sqrt(du2 * du2 + dv2 * dv2), shape, k.d0, n);
                    if (value == 0.0) break;
                }
                h[static_cast<size_t>(u) * Q + v] = static_cast<float>(value);
            }
        }
        return h;
    }

}

std::vector<float> makeFilter(int P, int Q, const FilterParams& p) {
    if (P <= 0 || Q <= 0) return {};
    const int n = std::max(1, p.order);

    if (p.kind == FilterKind::NotchReject || p.kind == FilterKind::NotchPass) {
        std::vector<float> h = notchReject(P, Q, p.notches, p.shape, n);
        if (p.kind == FilterKind::NotchPass) {
            for (float& v : h) v = 1.0f - v;                            // Eq. 4.10-6
        }
        return h;
    }

    std::vector<float> h(static_cast<size_t>(P) * static_cast<size_t>(Q), 1.0f);
    const double cu = P / 2.0, cv = Q / 2.0;
    for (int u = 0; u < P; ++u) {
        for (int v = 0; v < Q; ++v) {
            const double du = u - cu, dv = v - cv;
            const double D = std::sqrt(du * du + dv * dv);
            double value = 1.0;
            switch (p.kind) {
                case FilterKind::LowPass:
                    value = lowPassValue(D, p.shape, p.d0, n);
                    break;
                case FilterKind::HighPass:
                    value = 1.0 - lowPassValue(D, p.shape, p.d0, n);    // Eq. 4.9-1
                    break;
                case FilterKind::BandReject:
                    value = bandRejectValue(D, p.shape, p.d0, p.width, n);
                    break;
                case FilterKind::BandPass:
                    value = 1.0 - bandRejectValue(D, p.shape, p.d0, p.width, n);   // Eq. 4.10-1
                    break;
                default:
                    break;
            }
            h[static_cast<size_t>(u) * Q + v] = static_cast<float>(std::min(1.0, std::max(0.0, value)));
        }
    }
    return h;
}

std::vector<float> makeNotchRejectAt(int P, int Q, double row, double col,
                                     double d0, int order, FilterShape shape) {
    if (P <= 0 || Q <= 0) return {};
    Notch k;
    k.u = row - P / 2.0;
    k.v = col - Q / 2.0;
    k.d0 = d0;
    return notchReject(P, Q, {k}, shape, std::max(1, order));
}

}
