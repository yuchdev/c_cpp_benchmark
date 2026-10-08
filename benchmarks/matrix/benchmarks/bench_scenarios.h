/*
 * Shared definitions for the matrix "scenario" benchmarks.
 *
 * Valid in both C11 and C++17.  Everything the C and C++ drivers must agree on
 * lives here, so a pair can never silently drift apart:
 *
 *   - the sweep points of every scenario (same x-axis for both languages),
 *   - the exact row names that reach the CSV (the Python tooling pairs C and
 *     C++ rows by name),
 *   - the optimization barriers used inside the timed loops.
 *
 * Row-name grammar (see scripts/run_all_benchmarks.py::classify_matrix_row):
 *
 *     <lang>_<scenario>_<variant>[_n<param>]_<rows>x<cols>
 *
 *     c_chain_add_n256_8x256      chain of 8 terms on 256x256 matrices (variant add_n256)
 *     cpp_fixed_mul_4x4           compile-time 4x4 multiply
 *     cpp_block_mul_n512_64x64    64x64 block product inside 512x512 matrices
 *
 * bench_scn_name() below is the single place these names are produced.
 *
 * The sweep parameter is always `rows`; `cols` and the optional `_n<param>`
 * carry the secondary dimension.
 */
#ifndef BENCH_SCENARIOS_H
#define BENCH_SCENARIOS_H

#include <stddef.h>
#include <stdio.h>
#include "bench_options.h"

/* ---- Optimization barriers (Google-Benchmark style) -------------------------------
 * BENCH_ESCAPE(p)   makes the object at `p` observable to opaque code, so the
 *                   compiler can no longer prove it is private to the loop.
 * BENCH_CLOBBER()   after every timed call: pretends opaque code read and wrote all
 *                   escaped memory, so results must really be stored and inputs
 *                   reloaded each iteration instead of being hoisted out of the loop.
 * Without them an optimizer may legally collapse a timed loop over loop-invariant
 * tiny matrices into (almost) nothing. */
#if defined(__GNUC__) || defined(__clang__)
#define BENCH_ESCAPE(p) __asm__ __volatile__("" : : "g"(p) : "memory")
#define BENCH_CLOBBER() __asm__ __volatile__("" : : : "memory")
#else
static volatile const void *bench__escape_sink;
#define BENCH_ESCAPE(p) (bench__escape_sink = (const void *)(p))
#define BENCH_CLOBBER() ((void)0)
#endif

/* ---- Sweep definitions -------------------------------------------------------------- */

/* chain: A1 + ... + Ak on N x N matrices, k = MIN_K .. MAX_K, for several N.  Sweeping N
 * as well keeps the picture machine-independent: small N stays cache-resident (fusion
 * saves real load/store traffic), large N is DRAM-bound (it mostly cannot). */
#define BENCH_CHAIN_MIN_K  2
#define BENCH_CHAIN_MAX_K  16
static const size_t BENCH_CHAIN_SIZES[] = {32, 64, 128, 256, 512};
#define BENCH_NUM_CHAIN_SIZES ((int)(sizeof(BENCH_CHAIN_SIZES) / sizeof(BENCH_CHAIN_SIZES[0])))

/* fixed: compile-time N = MATRIX_FIXED_MIN_N .. MATRIX_FIXED_MAX_N (c_matrix/fixed.h). */
#define BENCH_FIXED_MIN_N  2
#define BENCH_FIXED_MAX_N  16

/* cliff: per-element time of streaming-ish kernels from L1-resident to DRAM-resident. */
static const size_t BENCH_CLIFF_SIZES[] = {16, 32, 64, 128, 256, 512, 1024, 2048};
#define BENCH_NUM_CLIFF_SIZES ((int)(sizeof(BENCH_CLIFF_SIZES) / sizeof(BENCH_CLIFF_SIZES[0])))
static const char *const BENCH_CLIFF_OPS[] = {"add", "transpose", "matvec"};
#define BENCH_NUM_CLIFF_OPS ((int)(sizeof(BENCH_CLIFF_OPS) / sizeof(BENCH_CLIFF_OPS[0])))

/* batch: one 4x4 transform applied to M contiguous 4-component points. */
static const size_t BENCH_BATCH_POINTS[] = {1024, 4096, 16384, 65536, 262144, 1048576};
#define BENCH_NUM_BATCH_POINTS ((int)(sizeof(BENCH_BATCH_POINTS) / sizeof(BENCH_BATCH_POINTS[0])))

/* block: bs x bs blocks inside N x N matrices; the three blocks sit at odd offsets
 * so no variant benefits from lucky alignment. */
#define BENCH_BLOCK_N 512
static const size_t BENCH_BLOCK_SIZES[] = {4, 8, 16, 32, 64, 128, 256};
#define BENCH_NUM_BLOCK_SIZES ((int)(sizeof(BENCH_BLOCK_SIZES) / sizeof(BENCH_BLOCK_SIZES[0])))
#define BENCH_BLOCK_A_ROW 3
#define BENCH_BLOCK_A_COL 5
#define BENCH_BLOCK_B_ROW 11
#define BENCH_BLOCK_B_COL 7
#define BENCH_BLOCK_O_ROW 1
#define BENCH_BLOCK_O_COL 9

/* tri: triangular solve (trsm) and symmetric rank-k update (syrk) on N x N. */
static const size_t BENCH_TRI_SIZES[] = {16, 32, 64, 128, 256};
#define BENCH_NUM_TRI_SIZES ((int)(sizeof(BENCH_TRI_SIZES) / sizeof(BENCH_TRI_SIZES[0])))

/* conv: 3x3 Gaussian blur of an N x N image; the IMAGE_N run also dumps PGM files. */
static const size_t BENCH_CONV_SIZES[] = {64, 128, 256, 512, 1024};
#define BENCH_NUM_CONV_SIZES ((int)(sizeof(BENCH_CONV_SIZES) / sizeof(BENCH_CONV_SIZES[0])))
#define BENCH_CONV_IMAGE_N 256
/* Row-major 3x3 Gaussian weights (1 2 1 / 2 4 2 / 1 2 1) / 16, identical in both drivers. */
#define BENCH_CONV_KERNEL_INIT                                                                  \
    {1.0 / 16.0, 2.0 / 16.0, 1.0 / 16.0, 2.0 / 16.0, 4.0 / 16.0,                                \
     2.0 / 16.0, 1.0 / 16.0, 2.0 / 16.0, 1.0 / 16.0}

/* ---- Row names ----------------------------------------------------------------------- */

/* Full CSV row name: "<lang>_<scenario>_<variant>[_n<param>]_<rows>x<cols>"; the
 * "_n<param>" part is present only when param >= 0. */
static inline void bench_scn_name(char *buf, size_t cap, const char *lang,
                                  const char *scenario, const char *variant, long param,
                                  size_t rows, size_t cols) {
    if (param >= 0)
        snprintf(buf, cap, "%s_%s_%s_n%ld_%zux%zu", lang, scenario, variant, param, rows, cols);
    else
        snprintf(buf, cap, "%s_%s_%s_%zux%zu", lang, scenario, variant, rows, cols);
}

/* ---- Work estimates (drive bench_iters_scaled; shared so both drivers agree) --------- */

/* Rough operation count of one core op on n x n matrices: O(n^3) for the GEMM family,
 * O(n^2) otherwise. */
static inline double bench_work_op(const char *op, double n) {
    return bench_op_is_heavy(op) ? n * n * n : n * n;
}
/* Iteration count for the core op x size grid: the usual --iters / --heavy-divisor
 * count (bench_iters_for), multiplied up for sizes whose single call is too cheap to
 * time reliably.  Large sizes are unchanged. */
static inline int bench_iters_for_core(const BenchOptions *o, const char *op, size_t n) {
    double it = (double)bench_iters_for(o, op, n) *
                bench_scale_for_work(bench_work_op(op, (double)n));
    if (it > 2.0e9) it = 2.0e9;
    return (int)it;
}
static inline double bench_work_chain(int k, double n) { return (double)k * n * n; }
static inline double bench_work_block(const char *variant, double bs) {
    return strcmp(variant, "mul") == 0 ? bs * bs * bs : bs * bs;
}
static inline double bench_work_tri(double n) { return n * n * n; }
static inline double bench_work_conv(double n) { return 9.0 * n * n; }
static inline double bench_work_batch(double points) { return 16.0 * points; }

/* Warmup iterations: 1 suffices once a single call is already large. */
static inline int bench_warmup_for(const BenchOptions *o, double work) {
    return (work >= 4194304.0 && o->warmup > 1) ? 1 : o->warmup;
}

#endif /* BENCH_SCENARIOS_H */
