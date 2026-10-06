#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <vector>

#if defined(_MSC_VER)
#define NOINLINE __declspec(noinline)
#else
#define NOINLINE __attribute__((noinline))
#endif

/*
 * Group "f" benchmark: compile-time-sized kernel unrolling.
 *
 * Same real-world scenario as the C version (f_fir_runtime_c.c): a fixed-
 * length FIR filter (weighted sliding-window sum) is applied to a large
 * input signal.
 *
 * The C version's reusable API takes the tap count as a *runtime* parameter,
 * so the compiler must emit one generic loop that works for any n_taps --
 * it cannot unroll the inner multiply-accumulate chain or keep taps resident
 * across iterations.
 *
 * Here the same reusable idea -- "one fir_apply usable for any filter
 * length" -- is expressed as a function template with the tap count as a
 * compile-time, non-type template parameter `N`. The compiler instantiates a
 * distinct specialization per N, fully unrolls the N-tap inner loop for that
 * specialization, keeps the (now compile-time-constant-indexed) taps in
 * registers, and auto-vectorizes the unrolled accumulation. Genericity over N
 * is preserved -- a new filter length is just a new template instantiation --
 * but each instantiation pays zero runtime cost for that genericity.
 */

#ifndef N_TAPS
#define N_TAPS 7
#endif

volatile double sink;

/*
 * Compile-time-sized FIR: N is baked in at compile time, so the compiler can
 * fully unroll the inner tap loop and vectorize the accumulation for this
 * specific N -- a new N simply instantiates a new specialization.
 */
template <std::size_t N>
NOINLINE static void fir_apply(const double* __restrict in, double* __restrict out,
                               std::size_t len, const std::array<double, N>& taps) {
    for (std::size_t i = 0; i + N <= len; ++i) {
        double acc = 0.0;
        for (std::size_t k = 0; k < N; ++k) {
            acc += in[i + k] * taps[k];
        }
        out[i] = acc;
    }
}

int main(int argc, char** argv) {
    std::size_t n = 10000000; /* default input length -- must match the C version */
    if (argc > 1) {
        try {
            long long v = std::stoll(argv[1]);
            if (v <= 0) return 1;
            n = static_cast<std::size_t>(v);
        } catch (...) {
            return 1;
        }
    }

    std::vector<double> in(n);
    std::vector<double> out(n);

    /* Deterministic fill -- byte-for-byte the same closed form as the C version. */
    for (std::size_t i = 0; i < n; ++i) {
        in[i] = static_cast<double>((i * 2654435761u) & 0xFFFF) * (1.0 / 65536.0) - 0.5;
    }

    /* Fixed tap coefficients, identical to the C version. */
    std::array<double, N_TAPS> taps{};
    for (std::size_t k = 0; k < N_TAPS; ++k) {
        taps[k] = 1.0 / static_cast<double>(k + 1);
    }

    auto t0 = std::chrono::high_resolution_clock::now();
    fir_apply<N_TAPS>(in.data(), out.data(), n, taps);
    auto t1 = std::chrono::high_resolution_clock::now();

    double acc = 0.0;
    for (std::size_t i = 0; i + N_TAPS <= n; ++i) {
        acc += out[i];
    }
    sink = acc; /* store result so the loop is not elided */

    std::chrono::duration<double> diff = t1 - t0;
    std::cout << "cpp fir measure=" << n << " time=" << diff.count() << " sec" << std::endl;

    return 0;
}
