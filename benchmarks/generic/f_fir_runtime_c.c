#define _POSIX_C_SOURCE 199309L
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#if defined(_MSC_VER)
#define NOINLINE __declspec(noinline)
#else
#define NOINLINE __attribute__((noinline))
#endif

/*
 * Group "f" benchmark: compile-time-sized kernel unrolling.
 *
 * Both programs apply a fixed-length FIR filter (a weighted sliding-window
 * sum -- the inner loop of any convolution/filtering/smoothing routine) to a
 * large input signal.
 *
 * The idiomatic, reusable C API takes the tap count as a *runtime* parameter
 * (`n_taps`), exactly like a real-world signal-processing library (e.g. a
 * generic `fir_apply(in, out, len, taps, n_taps)`) that must serve callers
 * with different filter lengths from one compiled function. Because n_taps
 * is only known at run time, the compiler cannot unroll the inner tap loop,
 * cannot keep the tap coefficients in registers across iterations, and
 * cannot fuse the multiply-accumulate chain into wide SIMD without a runtime
 * trip-count check. Every output sample re-walks a runtime-bounded loop.
 *
 * The matching C++ program (f_fir_template_cpp.cpp) expresses the same
 * reusable API as a function template `fir_apply<N>`, with the tap count as
 * a compile-time, non-type template parameter. The compiler instantiates one
 * fully-unrolled specialization per N, keeps the (compile-time-known) taps
 * resident, and auto-vectorizes the unrolled multiply-accumulate chain --
 * all without the caller giving up genericity over N (a new N just
 * instantiates a new specialization).
 */

#ifndef N_TAPS
#define N_TAPS 7
#endif

volatile double sink;

/*
 * Idiomatic, reusable FIR: n_taps is a runtime value, so this single compiled
 * function must serve any filter length -- the compiler cannot unroll or
 * specialize the inner loop for a specific tap count.
 */
NOINLINE static void fir_apply(const double* in, double* out, size_t len,
                               const double* taps, size_t n_taps) {
    for (size_t i = 0; i + n_taps <= len; ++i) {
        double acc = 0.0;
        for (size_t k = 0; k < n_taps; ++k) {
            acc += in[i + k] * taps[k];
        }
        out[i] = acc;
    }
}

static double now_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

int main(int argc, char* argv[]) {
    size_t n = 10000000; /* default input length */
    if (argc > 1) {
        long long v = atoll(argv[1]);
        if (v <= 0) return 1;
        n = (size_t)v;
    }

    double* in = (double*)malloc(n * sizeof(double));
    double* out = (double*)malloc(n * sizeof(double));
    if (!in || !out) {
        fprintf(stderr, "malloc failed\n");
        free(in);
        free(out);
        return 1;
    }

    /* Deterministic fill -- identical closed form in C and C++ (no RNG). */
    for (size_t i = 0; i < n; ++i) {
        in[i] = (double)((i * 2654435761u) & 0xFFFF) * (1.0 / 65536.0) - 0.5;
    }

    /* Fixed tap coefficients, must match the C++ version exactly. */
    double taps[N_TAPS];
    for (size_t k = 0; k < N_TAPS; ++k) {
        taps[k] = 1.0 / (double)(k + 1);
    }

    double t0 = now_sec();
    fir_apply(in, out, n, taps, N_TAPS);
    double t1 = now_sec();

    double acc = 0.0;
    for (size_t i = 0; i + N_TAPS <= n; ++i) {
        acc += out[i];
    }
    sink = acc; /* store result so the loop is not elided */

    printf("c fir measure=%zu time=%.6f sec\n", n, t1 - t0);

    free(in);
    free(out);
    return 0;
}
