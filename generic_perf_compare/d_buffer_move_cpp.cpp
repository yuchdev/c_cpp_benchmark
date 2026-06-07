#define _POSIX_C_SOURCE 200809L
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <array>
#include <vector>
#include <string>
#include <iostream>
#include <chrono>

#if defined(_MSC_VER)
#define NOINLINE __declspec(noinline)
#else
#define NOINLINE __attribute__((noinline))
#endif

/*
 * Group "d" benchmark: cache-locality, Array-of-Structs (AoS) vs Structure-of-Arrays (SoA).
 *
 * This C++ version uses a zero-cost Structure-of-Arrays view: the hot field lives
 * in its own contiguous std::vector<float>, and the cold payload lives in a
 * separate, parallel array the hot loop never touches. Reducing over the hot field
 * walks a contiguous N * 4 B buffer (~1 MB for the default N), which stays
 * L3/L2-resident and auto-vectorizes. The ergonomic abstraction yields a
 * cache-optimal layout for free, whereas the idiomatic C struct array (AoS) forces
 * a memory-bound layout.
 *
 * See docs/benchmark_d_redesign.md for the full design and the matching C (AoS)
 * version (d_buffer_copy_c.c).
 */

/* PASSES: how many times the array is traversed. Internal, machine-tuned so the
 * C (AoS) run lands in ~0.2-1.0 s at the default N. Must match the C version. */
#ifndef PASSES
#define PASSES 96
#endif

/* Cold fields stored separately; the hot field is contiguous.
 * This is the ergonomic, zero-cost "SoA view" of the same records. */
struct RecordSoA {
    std::vector<float>                  hot;   /* contiguous, ~N*4 B -> fits L3 */
    std::vector<std::array<uint8_t,60>> cold;  /* parallel, untouched by hot loop */

    explicit RecordSoA(std::size_t n) : hot(n), cold(n) {}
    std::size_t size() const { return hot.size(); }
};

/* The cold element is the same 60 bytes as the C record's padding, so the two
 * programs store byte-for-byte equivalent data -- only the layout differs. */
static_assert(sizeof(std::array<uint8_t,60>) == 60, "cold payload must be 60 B");

volatile double sink;          /* anti-dead-code-elimination, mirrors the C version */

template <class Reduce>
NOINLINE double traverse_soa(const float* hot, std::size_t n, int passes, Reduce r) {
    double acc = 0.0;
    for (int p = 0; p < passes; ++p) {
        for (std::size_t i = 0; i < n; ++i) {
            acc = r(acc, hot[i]);          /* contiguous 4 B stride -> vectorizes */
        }
    }
    return acc;                            /* working set = n * 4 B (L3/L2 resident) */
}

int main(int argc, char** argv) {
    std::size_t n = 262144;                /* default N (~1 MB hot set) */
    if (argc > 1) {
        try {
            long long v = std::stoll(argv[1]);
            if (v <= 0) return 1;
            n = static_cast<std::size_t>(v);
        } catch (...) {
            return 1;
        }
    }
    const int passes = PASSES;             /* same constant value as the C version */

    RecordSoA recs(n);
    /* Deterministic fill -- byte-for-byte the same closed form as the C version. */
    for (std::size_t i = 0; i < n; ++i) {
        recs.hot[i] = static_cast<float>((i * 2654435761u) & 0xFFFF) * (1.0f / 65536.0f);
    }

    auto t0 = std::chrono::high_resolution_clock::now();
    double acc = traverse_soa(recs.hot.data(), n, passes,
                              [](double a, float v) { return a + (double)v; });
    auto t1 = std::chrono::high_resolution_clock::now();

    sink = acc;                            /* store result so the loop is not elided */
    std::chrono::duration<double> diff = t1 - t0;
    std::cout << "cpp soa_traversal measure=" << n
              << " time=" << diff.count() << " sec" << std::endl;
    return 0;
}
