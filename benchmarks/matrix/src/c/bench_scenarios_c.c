/*
 * C side of the matrix "scenario" benchmarks (chain, fixed, cliff, batch, block,
 * tri, conv).  Counterpart of bench_scenarios_cpp.cpp: same sweeps (from
 * bench_scenarios.h), same data, same iteration counts, same row names.
 *
 * Every timed region is wrapped in BENCH_MEASURE, a macro (not a function-pointer
 * callback) so the call is visible to the optimizer exactly like the lambda the
 * C++ driver times -- the only remaining difference is the language and the
 * -O2 vs -O3/-march=native flags the suite documents.
 */
#include "bench_scenarios_c.h"
#include "bench_scenarios.h"
#include "c_matrix/fixed.h"
#include "c_matrix/matrix.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

_Static_assert(BENCH_FIXED_MIN_N == MATRIX_FIXED_MIN_N && BENCH_FIXED_MAX_N == MATRIX_FIXED_MAX_N,
               "bench_scenarios.h and c_matrix/fixed.h disagree on the fixed-size range");

static double bench_now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1e9 + (double)ts.tv_nsec;
}

/*
 * Best-of-`repeats` per-iteration average (ns) of the statements in __VA_ARGS__,
 * with an optimization barrier after every call.  Warmup runs each repeat, matching
 * run_once() in the core driver.
 */
#define BENCH_MEASURE(best, warmup, iters, repeats, ...)                                       \
    do {                                                                                       \
        (best) = -1.0;                                                                         \
        for (int bm_r_ = 0; bm_r_ < (repeats); ++bm_r_) {                                      \
            for (int bm_w_ = 0; bm_w_ < (warmup); ++bm_w_) {                                   \
                __VA_ARGS__;                                                                   \
                BENCH_CLOBBER();                                                               \
            }                                                                                  \
            const double bm_t0_ = bench_now_ns();                                              \
            for (int bm_i_ = 0; bm_i_ < (iters); ++bm_i_) {                                    \
                __VA_ARGS__;                                                                   \
                BENCH_CLOBBER();                                                               \
            }                                                                                  \
            const double bm_t_ = (bench_now_ns() - bm_t0_) / (double)(iters);                  \
            if ((best) < 0.0 || bm_t_ < (best))                                                \
                (best) = bm_t_;                                                                \
        }                                                                                      \
    } while (0)

/* ---- chain: A1 + A2 + ... + Ak, composed from binary matrix_add passes ------------------- */

static void run_chain(BenchReport *rp, const BenchOptions *o) {
    for (int si = 0; si < BENCH_NUM_CHAIN_SIZES; ++si) {
        const size_t n = BENCH_CHAIN_SIZES[si];
        Matrix terms[BENCH_CHAIN_MAX_K];
        const Matrix *ptrs[BENCH_CHAIN_MAX_K];
        for (int i = 0; i < BENCH_CHAIN_MAX_K; ++i) {
            terms[i] = matrix_create(n, n);
            matrix_fill_rand(&terms[i], o->seed + (unsigned int)i);
            ptrs[i] = &terms[i];
            BENCH_ESCAPE(terms[i].data);
        }
        Matrix tmp_a = matrix_create(n, n), tmp_b = matrix_create(n, n), out = matrix_create(n, n);
        BENCH_ESCAPE(tmp_a.data); BENCH_ESCAPE(tmp_b.data); BENCH_ESCAPE(out.data);

        for (int k = BENCH_CHAIN_MIN_K; k <= BENCH_CHAIN_MAX_K; ++k) {
            const double work = bench_work_chain(k, (double)n);
            const int iters = bench_iters_scaled(o, work);
            const int warmup = bench_warmup_for(o, work);
            double t;
            BENCH_MEASURE(t, warmup, iters, o->repeats,
                          matrix_add_chain(ptrs, (size_t)k, &tmp_a, &tmp_b, &out));
            char name[96];
            bench_scn_name(name, sizeof(name), "c", "chain", "add", (long)n, (size_t)k, n);
            bench_report_row(rp, name, (size_t)k, n, iters, t);
        }
        volatile double sink = out.data[0];
        (void)sink;
        for (int i = 0; i < BENCH_CHAIN_MAX_K; ++i)
            matrix_destroy(&terms[i]);
        matrix_destroy(&tmp_a); matrix_destroy(&tmp_b); matrix_destroy(&out);
    }
}

/* ---- fixed: compile-time N = 2..16 (one instantiation of bench_fixed_n.inc per N) -------- */

#define BENCH_FIXED_RUN_N 2
#include "bench_fixed_n.inc"
#undef BENCH_FIXED_RUN_N
#define BENCH_FIXED_RUN_N 3
#include "bench_fixed_n.inc"
#undef BENCH_FIXED_RUN_N
#define BENCH_FIXED_RUN_N 4
#include "bench_fixed_n.inc"
#undef BENCH_FIXED_RUN_N
#define BENCH_FIXED_RUN_N 5
#include "bench_fixed_n.inc"
#undef BENCH_FIXED_RUN_N
#define BENCH_FIXED_RUN_N 6
#include "bench_fixed_n.inc"
#undef BENCH_FIXED_RUN_N
#define BENCH_FIXED_RUN_N 7
#include "bench_fixed_n.inc"
#undef BENCH_FIXED_RUN_N
#define BENCH_FIXED_RUN_N 8
#include "bench_fixed_n.inc"
#undef BENCH_FIXED_RUN_N
#define BENCH_FIXED_RUN_N 9
#include "bench_fixed_n.inc"
#undef BENCH_FIXED_RUN_N
#define BENCH_FIXED_RUN_N 10
#include "bench_fixed_n.inc"
#undef BENCH_FIXED_RUN_N
#define BENCH_FIXED_RUN_N 11
#include "bench_fixed_n.inc"
#undef BENCH_FIXED_RUN_N
#define BENCH_FIXED_RUN_N 12
#include "bench_fixed_n.inc"
#undef BENCH_FIXED_RUN_N
#define BENCH_FIXED_RUN_N 13
#include "bench_fixed_n.inc"
#undef BENCH_FIXED_RUN_N
#define BENCH_FIXED_RUN_N 14
#include "bench_fixed_n.inc"
#undef BENCH_FIXED_RUN_N
#define BENCH_FIXED_RUN_N 15
#include "bench_fixed_n.inc"
#undef BENCH_FIXED_RUN_N
#define BENCH_FIXED_RUN_N 16
#include "bench_fixed_n.inc"
#undef BENCH_FIXED_RUN_N

static void run_fixed(BenchReport *rp, const BenchOptions *o) {
#define BENCH_FIXED_CALL(N) bench_fixed_run_##N(rp, o);
    MATRIX_FIXED_FOREACH_SIZE(BENCH_FIXED_CALL)
#undef BENCH_FIXED_CALL
}

/* ---- cliff: per-element time from L1-resident to DRAM-resident matrices ------------------- */

static void run_cliff(BenchReport *rp, const BenchOptions *o) {
    for (int si = 0; si < BENCH_NUM_CLIFF_SIZES; ++si) {
        const size_t n = BENCH_CLIFF_SIZES[si];
        Matrix a = matrix_create(n, n), b = matrix_create(n, n), out = matrix_create(n, n);
        matrix_fill_rand(&a, o->seed);
        matrix_fill_rand(&b, o->seed + 1u);
        double *x = (double *)malloc(n * sizeof(double));
        double *y = (double *)malloc(n * sizeof(double));
        for (size_t i = 0; i < n; ++i) {
            x[i] = (double)i * 0.001;
            y[i] = 0.0;
        }
        BENCH_ESCAPE(a.data); BENCH_ESCAPE(b.data); BENCH_ESCAPE(out.data);
        BENCH_ESCAPE(x); BENCH_ESCAPE(y);

        for (int oi = 0; oi < BENCH_NUM_CLIFF_OPS; ++oi) {
            const char *op = BENCH_CLIFF_OPS[oi];
            if (!bench_op_enabled(o, op))
                continue;
            const double work = bench_work_op(op, (double)n);
            const int iters = bench_iters_scaled(o, work);
            const int warmup = bench_warmup_for(o, work);
            double t = 0.0;
            if (strcmp(op, "add") == 0)
                BENCH_MEASURE(t, warmup, iters, o->repeats, matrix_add(&a, &b, &out));
            else if (strcmp(op, "transpose") == 0)
                BENCH_MEASURE(t, warmup, iters, o->repeats, matrix_transpose(&a, &out));
            else
                BENCH_MEASURE(t, warmup, iters, o->repeats, matrix_matvec(&a, x, y));
            char name[96];
            bench_scn_name(name, sizeof(name), "c", "cliff", op, -1, n, n);
            bench_report_row(rp, name, n, n, iters, t);
        }
        volatile double sink = out.data[0] + y[0];
        (void)sink;
        matrix_destroy(&a); matrix_destroy(&b); matrix_destroy(&out);
        free(x); free(y);
    }
}

/* ---- batch: one 4x4 transform applied to M contiguous points ------------------------------ */

static void run_batch(BenchReport *rp, const BenchOptions *o) {
    Matrix t4 = matrix_create(4, 4);
    matrix_fill_rand(&t4, o->seed);
    BENCH_ESCAPE(t4.data);
    for (int pi = 0; pi < BENCH_NUM_BATCH_POINTS; ++pi) {
        const size_t m = BENCH_BATCH_POINTS[pi];
        Matrix pts = matrix_create(m, 4), out = matrix_create(m, 4);
        matrix_fill_rand(&pts, o->seed + 1u);
        BENCH_ESCAPE(pts.data); BENCH_ESCAPE(out.data);

        const double work = bench_work_batch((double)m);
        const int iters = bench_iters_scaled(o, work);
        const int warmup = bench_warmup_for(o, work);
        double t;
        BENCH_MEASURE(t, warmup, iters, o->repeats, matrix_transform_points4(&t4, &pts, &out));
        char name[96];
        bench_scn_name(name, sizeof(name), "c", "batch", "xform4", -1, m, 4);
        bench_report_row(rp, name, m, 4, iters, t);

        volatile double sink = out.data[0];
        (void)sink;
        matrix_destroy(&pts); matrix_destroy(&out);
    }
    matrix_destroy(&t4);
}

/* ---- block: copy / multiply bs x bs submatrices of larger matrices ------------------------ */

static void run_block(BenchReport *rp, const BenchOptions *o) {
    const size_t n = BENCH_BLOCK_N;
    Matrix a = matrix_create(n, n), b = matrix_create(n, n), d = matrix_create(n, n);
    matrix_fill_rand(&a, o->seed);
    matrix_fill_rand(&b, o->seed + 1u);
    BENCH_ESCAPE(a.data); BENCH_ESCAPE(b.data); BENCH_ESCAPE(d.data);

    for (int bi = 0; bi < BENCH_NUM_BLOCK_SIZES; ++bi) {
        const size_t bs = BENCH_BLOCK_SIZES[bi];
        for (int vi = 0; vi < 2; ++vi) {
            const char *variant = vi == 0 ? "copy" : "mul";
            const double work = bench_work_block(variant, (double)bs);
            const int iters = bench_iters_scaled(o, work);
            const int warmup = bench_warmup_for(o, work);
            double t;
            if (vi == 0)
                BENCH_MEASURE(t, warmup, iters, o->repeats,
                              matrix_block_copy(&a, BENCH_BLOCK_A_ROW, BENCH_BLOCK_A_COL, &d,
                                                BENCH_BLOCK_O_ROW, BENCH_BLOCK_O_COL, bs));
            else
                BENCH_MEASURE(t, warmup, iters, o->repeats,
                              matrix_block_mul(&a, BENCH_BLOCK_A_ROW, BENCH_BLOCK_A_COL, &b,
                                               BENCH_BLOCK_B_ROW, BENCH_BLOCK_B_COL, &d,
                                               BENCH_BLOCK_O_ROW, BENCH_BLOCK_O_COL, bs));
            char name[96];
            bench_scn_name(name, sizeof(name), "c", "block", variant, (long)n, bs, bs);
            bench_report_row(rp, name, bs, bs, iters, t);
        }
    }
    volatile double sink = d.data[0];
    (void)sink;
    matrix_destroy(&a); matrix_destroy(&b); matrix_destroy(&d);
}

/* ---- tri: triangular solve and symmetric rank-k update ------------------------------------ */

static void run_tri(BenchReport *rp, const BenchOptions *o) {
    for (int si = 0; si < BENCH_NUM_TRI_SIZES; ++si) {
        const size_t n = BENCH_TRI_SIZES[si];
        Matrix a = matrix_create(n, n), rhs = matrix_create(n, n), l = matrix_create(n, n);
        Matrix x = matrix_create(n, n), s = matrix_create(n, n);
        matrix_fill_rand(&a, o->seed);
        matrix_fill_rand(&rhs, o->seed + 1u);
        matrix_make_lower_triangular(&a, &l);
        BENCH_ESCAPE(a.data); BENCH_ESCAPE(rhs.data); BENCH_ESCAPE(l.data);
        BENCH_ESCAPE(x.data); BENCH_ESCAPE(s.data);

        const double work = bench_work_tri((double)n);
        const int iters = bench_iters_scaled(o, work);
        const int warmup = bench_warmup_for(o, work);
        double t;
        char name[96];

        BENCH_MEASURE(t, warmup, iters, o->repeats, matrix_trsm_lower(&l, &rhs, &x));
        bench_scn_name(name, sizeof(name), "c", "tri", "trsm", -1, n, n);
        bench_report_row(rp, name, n, n, iters, t);

        BENCH_MEASURE(t, warmup, iters, o->repeats, matrix_syrk_lower(&a, &s));
        bench_scn_name(name, sizeof(name), "c", "tri", "syrk", -1, n, n);
        bench_report_row(rp, name, n, n, iters, t);

        volatile double sink = x.data[0] + s.data[0];
        (void)sink;
        matrix_destroy(&a); matrix_destroy(&rhs); matrix_destroy(&l);
        matrix_destroy(&x); matrix_destroy(&s);
    }
}

/* ---- conv: 3x3 Gaussian blur of a generated image ----------------------------------------- */

static void write_conv_images(const BenchOptions *o, const Matrix *in, const Matrix *out) {
    char path[BENCH_PATH_LEN + 32];
    snprintf(path, sizeof(path), "%s/conv_input_c.pgm", o->image_dir);
    if (matrix_write_pgm(in, path) != 0)
        fprintf(stderr, "warning: could not write %s\n", path);
    snprintf(path, sizeof(path), "%s/conv_output_c.pgm", o->image_dir);
    if (matrix_write_pgm(out, path) != 0)
        fprintf(stderr, "warning: could not write %s\n", path);
}

static void run_conv(BenchReport *rp, const BenchOptions *o) {
    const double kernel[9] = BENCH_CONV_KERNEL_INIT;
    for (int si = 0; si < BENCH_NUM_CONV_SIZES; ++si) {
        const size_t n = BENCH_CONV_SIZES[si];
        Matrix img = matrix_create(n, n), out = matrix_create(n - 2, n - 2);
        matrix_fill_image(&img, o->seed);
        BENCH_ESCAPE(img.data); BENCH_ESCAPE(out.data); BENCH_ESCAPE(kernel);

        const double work = bench_work_conv((double)n);
        const int iters = bench_iters_scaled(o, work);
        const int warmup = bench_warmup_for(o, work);
        double t;
        BENCH_MEASURE(t, warmup, iters, o->repeats, matrix_conv3x3(&img, kernel, &out));
        char name[96];
        bench_scn_name(name, sizeof(name), "c", "conv", "blur3x3", -1, n, n);
        bench_report_row(rp, name, n, n, iters, t);

        if (n == BENCH_CONV_IMAGE_N && o->image_dir[0])
            write_conv_images(o, &img, &out); /* after timing: no I/O in the timed region */
        volatile double sink = out.data[0];
        (void)sink;
        matrix_destroy(&img); matrix_destroy(&out);
    }
}

/* ---- dispatcher ---------------------------------------------------------------------------- */

void bench_run_scenarios_c(BenchReport *rp, const BenchOptions *o) {
    if (bench_scenario_enabled(o, BENCH_SCN_CHAIN)) {
        bench_report_section(rp, "Scenario: expression chain  A1 + ... + Ak  (k = 2..16, N = 32..512)");
        run_chain(rp, o);
    }
    if (bench_scenario_enabled(o, BENCH_SCN_FIXED)) {
        bench_report_section(rp, "Scenario: compile-time-sized matrices  (N = 2..16)");
        run_fixed(rp, o);
    }
    if (bench_scenario_enabled(o, BENCH_SCN_CLIFF)) {
        bench_report_section(rp, "Scenario: cache-cliff size sweep  (N = 16..2048)");
        run_cliff(rp, o);
    }
    if (bench_scenario_enabled(o, BENCH_SCN_BATCH)) {
        bench_report_section(rp, "Scenario: 4x4 transform over M points");
        run_batch(rp, o);
    }
    if (bench_scenario_enabled(o, BENCH_SCN_BLOCK)) {
        bench_report_section(rp, "Scenario: submatrix block copy / multiply");
        run_block(rp, o);
    }
    if (bench_scenario_enabled(o, BENCH_SCN_TRI)) {
        bench_report_section(rp, "Scenario: triangular solve + symmetric rank-k update");
        run_tri(rp, o);
    }
    if (bench_scenario_enabled(o, BENCH_SCN_CONV)) {
        bench_report_section(rp, "Scenario: 3x3 convolution (blur)");
        run_conv(rp, o);
    }
}
