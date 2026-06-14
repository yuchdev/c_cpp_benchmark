#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <time.h>

#if defined(_MSC_VER)
#define NOINLINE __declspec(noinline)
#else
#define NOINLINE __attribute__((noinline))
#endif

/*
 * Group "d" benchmark: cache-locality, Array-of-Structs (AoS) vs Structure-of-Arrays (SoA).
 *
 * This C version uses the natural, idiomatic C layout: an array of fat records,
 * one cache line each (64 B). The hot loop reads only the 4-byte "hot" field of
 * every record, but because the records are interleaved, the traversal strides a
 * full cache line per element -> the effective working set is N * 64 B, which
 * far exceeds L3 and makes the loop DRAM-bandwidth bound.
 *
 * See docs/benchmark_d_redesign.md for the full design and the matching C++ (SoA)
 * version (d_buffer_move_cpp.cpp).
 */

/* PASSES: how many times the array is traversed. Internal, machine-tuned so the
 * C (AoS) run lands in ~0.2-1.0 s at the default N. Must match the C++ version. */
#ifndef PASSES
#define PASSES 96
#endif

typedef struct Record {        /* 64 bytes == one cache line */
    float    hot;              /* the ONLY field the hot loop reads (4 B) */
    uint8_t  cold[60];         /* cold payload, never touched in the loop (60 B) */
} Record;

/* The whole effect depends on the 64 B stride, so make it a hard requirement. */
_Static_assert(sizeof(Record) == 64, "Record must be exactly one cache line");

volatile double sink;          /* anti-dead-code-elimination: forces the loop to run */

NOINLINE static double traverse_aos(const Record* recs, size_t n, int passes) {
    double acc = 0.0;
    for (int p = 0; p < passes; ++p) {
        for (size_t i = 0; i < n; ++i) {
            acc += (double)recs[i].hot;   /* strides 64 B -> 1 cache line / element */
        }
    }
    return acc;                            /* effective working set = n * 64 B (>> L3) */
}

static double now_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

int main(int argc, char* argv[]) {
    size_t n = 262144;                     /* default N (~1 MB SoA hot set) */
    if (argc > 1) {
        long long v = atoll(argv[1]);
        if (v <= 0) return 1;
        n = (size_t)v;
    }
    const int passes = PASSES;

    Record* recs = (Record*)malloc(n * sizeof(Record));
    if (!recs) {
        fprintf(stderr, "malloc failed\n");
        return 1;
    }

    /* Deterministic fill -- identical closed form in C and C++ (no RNG).
     * cold[] is left arbitrary on purpose; the hot loop never reads it. */
    for (size_t i = 0; i < n; ++i) {
        recs[i].hot = (float)((i * 2654435761u) & 0xFFFF) * (1.0f / 65536.0f);
    }

    double t0 = now_sec();
    double acc = traverse_aos(recs, n, passes);
    double t1 = now_sec();

    sink = acc;                            /* store result so the loop is not elided */
    printf("c soa_traversal measure=%zu time=%.6f sec\n", n, t1 - t0);

    free(recs);
    return 0;
}
