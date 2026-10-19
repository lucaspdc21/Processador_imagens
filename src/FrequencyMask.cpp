#include "FrequencyMask.hpp"
#include <algorithm>
#include <cmath>

namespace core {

namespace {
    // Limite de memoria do historico (em floats; antes + depois).
    constexpr size_t kMaxHistoryFloats = 64u * 1000u * 1000u;
    constexpr size_t kMaxHistoryEntries = 100;
}

void SpectrumMask::reset(int P, int Q) {
    rows_ = std::max(0, P);
    cols_ = std::max(0, Q);
    h_.assign(static_cast<size_t>(rows_) * static_cast<size_t>(cols_), 1.0f);
    snapshot_.clear();
    snapshot_.shrink_to_fit();
    editing_ = false;
    strokeBox_ = Rect();
    undo_.clear();
    redo_.clear();
}

double SpectrumMask::brushAlpha(double d, const Brush& b) {
    if (b.hardDisc) return d <= b.radius ? 1.0 : 0.0;
    const double hard = std::min(1.0, std::max(0.0, b.hardness));
    const double sigma = std::max(1e-6, b.radius * (1.0 - hard * 0.9));
    const double core = std::max(0.0, b.radius - 3.0 * sigma);
    const double t = std::max(0.0, d - core);
    return std::exp(-(t * t) / (2.0 * sigma * sigma));
}

int SpectrumMask::brushExtent(const Brush& b) {
    if (b.hardDisc) return static_cast<int>(std::ceil(std::max(0.5, b.radius)));
    const double hard = std::min(1.0, std::max(0.0, b.hardness));
    const double sigma = std::max(1e-6, b.radius * (1.0 - hard * 0.9));
    const double core = std::max(0.0, b.radius - 3.0 * sigma);
    return static_cast<int>(std::ceil(std::max(0.5, core + 3.0 * sigma)));
}

void SpectrumMask::copyRect(const std::vector<float>& src, int cols, const Rect& r, std::vector<float>& out) {
    out.resize(static_cast<size_t>(r.rows()) * static_cast<size_t>(r.cols()));
    for (int row = r.r0; row < r.r1; ++row) {
        std::copy(src.begin() + static_cast<std::ptrdiff_t>(row) * cols + r.c0,
                  src.begin() + static_cast<std::ptrdiff_t>(row) * cols + r.c1,
                  out.begin() + static_cast<std::ptrdiff_t>(row - r.r0) * r.cols());
    }
}

void SpectrumMask::writeRect(const Rect& r, const std::vector<float>& data) {
    for (int row = r.r0; row < r.r1; ++row) {
        std::copy(data.begin() + static_cast<std::ptrdiff_t>(row - r.r0) * r.cols(),
                  data.begin() + static_cast<std::ptrdiff_t>(row - r.r0 + 1) * r.cols(),
                  h_.begin() + static_cast<std::ptrdiff_t>(row) * cols_ + r.c0);
    }
}

void SpectrumMask::beginStroke() {
    if (!valid() || editing_) return;
    snapshot_ = h_;
    editing_ = true;
    strokeBox_ = Rect();
}

void SpectrumMask::stamp(double rc, double cc, const Brush& b, Rect& winRect, Rect& mirRect) {
    const int R = brushExtent(b);
    const int rlo = std::max(0, static_cast<int>(std::floor(rc)) - R);
    const int rhi = std::min(rows_ - 1, static_cast<int>(std::ceil(rc)) + R);
    const int clo = std::max(0, static_cast<int>(std::floor(cc)) - R);
    const int chi = std::min(cols_ - 1, static_cast<int>(std::ceil(cc)) + R);
    if (rlo > rhi || clo > chi) return;

    const double op = std::min(1.0, std::max(0.0, b.opacity));
    const float val = static_cast<float>(std::min(1.0, std::max(0.0, b.value)));

    auto inWindow = [&](int r, int c) { return r >= rlo && r <= rhi && c >= clo && c <= chi; };
    auto applyAt = [&](int r, int c, double a) {
        float& h = h_[static_cast<size_t>(r) * cols_ + c];
        const double k = a * op;
        h = static_cast<float>(h * (1.0 - k) + val * k);
    };

    for (int r = rlo; r <= rhi; ++r) {
        for (int c = clo; c <= chi; ++c) {
            // alfa do carimbo em (r,c) e do carimbo espelhado (= alfa do carimbo
            // original no ponto espelhado reduzido modulo P, Q)
            const int mr = (rows_ - r) % rows_;
            const int mc = (cols_ - c) % cols_;
            const double d1 = std::hypot(r - rc, c - cc);
            const double d2 = std::hypot(mr - rc, mc - cc);
            const double a1 = brushAlpha(d1, b);
            const double a2 = brushAlpha(d2, b);
            const double a = 1.0 - (1.0 - a1) * (1.0 - a2);       // combina antes de aplicar
            if (a <= 0.0) continue;

            applyAt(r, c, a);
            if ((mr != r || mc != c) && !inWindow(mr, mc)) {
                applyAt(mr, mc, a);                               // mesmo alfa no ponto espelhado
                mirRect.unite(Rect{mr, mc, mr + 1, mc + 1});
            }
        }
    }
    winRect.unite(Rect{rlo, clo, rhi + 1, chi + 1});
}

std::vector<Rect> SpectrumMask::paintSegment(double r0, double c0, double r1, double c1, const Brush& brush) {
    std::vector<Rect> out;
    if (!valid()) return out;
    if (!editing_) beginStroke();

    const double dist = std::hypot(r1 - r0, c1 - c0);
    const double spacing = std::max(1.0, 0.25 * brush.radius);
    // Segmento de comprimento zero = um unico carimbo (nao carimba duas vezes).
    const int steps = dist > 0.0 ? std::max(1, static_cast<int>(std::ceil(dist / spacing))) : 0;

    Rect win, mir;
    for (int i = 0; i <= steps; ++i) {
        // O ponto inicial de um segmento que continua o anterior ja foi
        // carimbado; so repete no primeiro segmento da pincelada (steps = 1
        // com dist = 0 carimba uma vez).
        if (i == 0 && dist > 0.0 && !strokeBox_.empty()) continue;
        const double t = steps > 0 ? static_cast<double>(i) / steps : 0.0;
        stamp(r0 + (r1 - r0) * t, c0 + (c1 - c0) * t, brush, win, mir);
    }
    strokeBox_.unite(win);
    strokeBox_.unite(mir);
    if (!win.empty()) out.push_back(win);
    if (!mir.empty()) out.push_back(mir);
    return out;
}

void SpectrumMask::commit(const Rect& bbox) {
    if (bbox.empty() || !editing_) {
        editing_ = false;
        snapshot_.clear();
        snapshot_.shrink_to_fit();
        return;
    }
    Entry e;
    e.rect = bbox;
    copyRect(snapshot_, cols_, bbox, e.before);
    copyRect(h_, cols_, bbox, e.after);
    undo_.push_back(std::move(e));
    redo_.clear();
    editing_ = false;
    snapshot_.clear();
    snapshot_.shrink_to_fit();
    trimHistory();
}

void SpectrumMask::trimHistory() {
    size_t total = 0;
    for (const Entry& e : undo_) total += e.before.size() + e.after.size();
    while (undo_.size() > 1 && (total > kMaxHistoryFloats || undo_.size() > kMaxHistoryEntries)) {
        total -= undo_.front().before.size() + undo_.front().after.size();
        undo_.erase(undo_.begin());
    }
}

Rect SpectrumMask::endStroke() {
    const Rect box = strokeBox_;
    commit(box);
    strokeBox_ = Rect();
    return box;
}

Rect SpectrumMask::multiplyBy(const std::vector<float>& filter) {
    if (!valid() || filter.size() != h_.size()) return Rect();
    beginStroke();
    for (size_t i = 0; i < h_.size(); ++i) h_[i] *= filter[i];
    const Rect all{0, 0, rows_, cols_};
    commit(all);
    return all;
}

Rect SpectrumMask::invert() {
    if (!valid()) return Rect();
    beginStroke();
    for (float& v : h_) v = 1.0f - v;
    const Rect all{0, 0, rows_, cols_};
    commit(all);
    return all;
}

Rect SpectrumMask::clear() {
    if (!valid()) return Rect();
    beginStroke();
    std::fill(h_.begin(), h_.end(), 1.0f);
    const Rect all{0, 0, rows_, cols_};
    commit(all);
    return all;
}

Rect SpectrumMask::undo() {
    if (undo_.empty() || editing_) return Rect();
    Entry e = std::move(undo_.back());
    undo_.pop_back();
    writeRect(e.rect, e.before);
    const Rect r = e.rect;
    redo_.push_back(std::move(e));
    return r;
}

Rect SpectrumMask::redo() {
    if (redo_.empty() || editing_) return Rect();
    Entry e = std::move(redo_.back());
    redo_.pop_back();
    writeRect(e.rect, e.after);
    const Rect r = e.rect;
    undo_.push_back(std::move(e));
    return r;
}

}
