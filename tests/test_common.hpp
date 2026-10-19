#pragma once
// Mini-framework de testes (sem dependencias externas).
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <string>
#include <vector>

namespace testing {

struct Case { std::string name; std::function<void()> fn; };

inline std::vector<Case>& registry() { static std::vector<Case> r; return r; }
inline int& failures() { static int f = 0; return f; }
inline int& checks() { static int c = 0; return c; }

struct Registrar {
    Registrar(const std::string& n, std::function<void()> f) { registry().push_back({n, std::move(f)}); }
};

inline int runAll() {
    int failedCases = 0;
    for (auto& c : registry()) {
        const int before = failures();
        c.fn();
        const bool ok = failures() == before;
        std::printf("[%s] %s\n", ok ? " OK " : "FAIL", c.name.c_str());
        if (!ok) ++failedCases;
    }
    std::printf("\n%zu testes, %d falharam, %d verificacoes.\n", registry().size(), failedCases, checks());
    return failedCases == 0 ? 0 : 1;
}

}

#define TEST(name) \
    static void test_##name(); \
    static testing::Registrar registrar_##name(#name, test_##name); \
    static void test_##name()

#define CHECK(cond) do { \
    ++testing::checks(); \
    if (!(cond)) { ++testing::failures(); \
        std::printf("    falhou: %s (linha %d)\n", #cond, __LINE__); } } while (0)

#define CHECK_NEAR(a, b, tol) do { \
    ++testing::checks(); \
    const double va_ = (a), vb_ = (b); \
    if (!(std::fabs(va_ - vb_) <= (tol))) { ++testing::failures(); \
        std::printf("    falhou: %s ~ %s  (%.12g vs %.12g, tol %.3g, linha %d)\n", #a, #b, va_, vb_, (double)(tol), __LINE__); } } while (0)
