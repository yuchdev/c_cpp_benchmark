#define _POSIX_C_SOURCE 199309L
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/*
 * Group "e" benchmark: runtime lookup-table transform vs compile-time constexpr
 * table fusion. (C side -- the realistic runtime version.)
 *
 * Real-world scenario: a byte-oriented substitution pipeline (think a codec /
 * cipher / hashing pre-stage) applies R rounds of an S-box substitution plus a
 * bit-rotation to every byte of a large buffer.
 *
 * In idiomatic C, the S-box is produced at runtime (here, derived from a seed
 * that is only known at run time, modelled with a `volatile` global so the
 * optimizer cannot fold it). Because the box contents are opaque, the compiler
 * MUST emit a real, data-dependent memory load for every round, and the R
 * rounds form a dependency chain per byte: R serial table loads per input byte.
 *
 * The matching C++ version (e_constexpr_table_cpp.cpp) declares the same S-box
 * and the same round function as `constexpr`, so the compiler FUSES all R rounds
 * into a single 256-entry table at *compile time*. At run time C++ does exactly
 * one lookup per byte, where C does R -- the compile-time-vs-runtime advantage.
 */

#if defined(_MSC_VER)
#define NOINLINE __declspec(noinline)
#else
#define NOINLINE __attribute__((noinline))
#endif

/* ROUNDS: number of substitution+rotate rounds applied per byte. The C version
 * pays this at run time; the C++ version folds it away at compile time. */
#ifndef ROUNDS
#define ROUNDS 24
#endif

/* Rotation amount used between substitution rounds (a relatively-prime-ish odd
 * value so the rounds genuinely mix). Must match the C++ version. */
#define ROUND_ROT 3

static uint32_t mix32(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

/*
 * Seed for the S-box. In real code this would come from a config file, a key,
 * or a runtime computation; we model that here with a `volatile` global so the
 * compiler is forced to treat the resulting S-box as opaque runtime data and
 * cannot constant-fold the rounds the way the C++ constexpr version does.
 */
volatile uint32_t g_seed = 0x9E3779B9u;

static uint8_t rotl8(uint8_t x, int r) {
    return (uint8_t)((x << r) | (x >> (8 - r)));
}

static void build_sbox(uint8_t sbox[256], uint32_t seed) {
    for (size_t i = 0; i < 256; ++i) {
        sbox[i] = (uint8_t)(mix32((uint32_t)i ^ seed) & 0xFFu);
    }
}

volatile uint32_t sink;

/*
 * Apply ROUNDS of (substitute, rotate) to every byte of the buffer and reduce.
 * Each byte runs through a serial chain of ROUNDS opaque table loads.
 */
NOINLINE static uint32_t transform_buffer(const uint8_t* data, size_t n,
                                          const uint8_t* sbox) {
    uint32_t acc = 0;
    for (size_t i = 0; i < n; ++i) {
        uint8_t x = data[i];
        for (int r = 0; r < ROUNDS; ++r) {
            x = sbox[x];              /* opaque, data-dependent load each round */
            x = rotl8(x, ROUND_ROT);
        }
        acc ^= (uint32_t)x;
    }
    return acc;
}

static double now_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

int main(int argc, char* argv[]) {
    size_t n = 16777216; /* default dataset: 16 MiB of bytes */
    if (argc > 1) {
        long long v = atoll(argv[1]);
        if (v <= 0) return 1;
        n = (size_t)v;
    }

    uint8_t* data = (uint8_t*)malloc(n);
    if (!data) {
        fprintf(stderr, "malloc failed\n");
        return 1;
    }
    /* Deterministic fill -- identical closed form in C and C++ (no RNG). */
    for (size_t i = 0; i < n; ++i) {
        data[i] = (uint8_t)((uint32_t)((i * 2654435761u) >> 24) & 0xFFu);
    }

    uint8_t sbox[256];
    build_sbox(sbox, g_seed); /* seed is runtime-opaque -> box is opaque */

    double t0 = now_sec();
    uint32_t acc = transform_buffer(data, n, sbox);
    double t1 = now_sec();

    sink = acc; /* store result so the loop is not elided */
    printf("c runtime_table measure=%zu time=%.6f sec\n", n, t1 - t0);

    free(data);
    return 0;
}
