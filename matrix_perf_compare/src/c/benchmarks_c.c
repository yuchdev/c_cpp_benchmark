/*
 * C matrix benchmark driver.
 *
 * Represents the "portable, hand-written C" baseline: straightforward
 * row-major double matrices with naive O(N^3) multiply and no
 * machine-specific tuning.  The flexible CLI is shared with the C++
 * driver via benchmarks/bench_options.h.
 */
#include "c_matrix/matrix.h"
#include "bench_options.h"
#include "bench_report.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

/* ---- timing ---- */
static double now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1e9 + (double)ts.tv_nsec;
}

/* ---- benchmark runner ---- */
typedef void (*bench_fn)(void *ctx);

static double run_once(bench_fn fn, void *ctx, int warmup, int iters) {
    for (int i = 0; i < warmup; ++i) fn(ctx);
    double t0 = now_ns();
    for (int i = 0; i < iters; ++i) fn(ctx);
    double t1 = now_ns();
    return (t1 - t0) / iters;
}

/* Best-of-repeats average (minimises OS scheduling jitter). */
static double run_benchmark(bench_fn fn, void *ctx, int warmup, int iters, int repeats) {
    double best = -1.0;
    for (int r = 0; r < repeats; ++r) {
        double t = run_once(fn, ctx, warmup, iters);
        if (best < 0.0 || t < best) best = t;
    }
    return best;
}

/* ---- per-operation contexts ---- */
typedef struct { Matrix a; Matrix out; } CtxTranspose;
typedef struct { Matrix a, b, out; } CtxBinary;
typedef struct { Matrix a; double s; Matrix out; } CtxScale;
typedef struct { Matrix a; double *x; double *y; } CtxMatVec;
typedef struct { Matrix a, b, c, out; } CtxTernary;

static void bench_transpose(void *p)     { CtxTranspose *c = (CtxTranspose *)p; matrix_transpose(&c->a, &c->out); }
static void bench_add(void *p)           { CtxBinary *c = (CtxBinary *)p; matrix_add(&c->a, &c->b, &c->out); }
static void bench_sub(void *p)           { CtxBinary *c = (CtxBinary *)p; matrix_sub(&c->a, &c->b, &c->out); }
static void bench_scale(void *p)         { CtxScale *c = (CtxScale *)p; matrix_scale(&c->a, c->s, &c->out); }
static void bench_matvec(void *p)        { CtxMatVec *c = (CtxMatVec *)p; matrix_matvec(&c->a, c->x, c->y); }
static void bench_mul(void *p)           { CtxBinary *c = (CtxBinary *)p; matrix_mul(&c->a, &c->b, &c->out); }
static void bench_transpose_mul(void *p) { CtxBinary *c = (CtxBinary *)p; matrix_transpose_mul(&c->a, &c->b, &c->out); }
static void bench_add3(void *p)          { CtxTernary *c = (CtxTernary *)p; matrix_add3(&c->a, &c->b, &c->c, &c->out); }
static void bench_mul_add(void *p)       { CtxTernary *c = (CtxTernary *)p; matrix_mul_add(&c->a, &c->b, &c->c, &c->out); }

/* ---- run one operation at one size ---- */
static void run_op(BenchReport *rp, const BenchOptions *o,
                   const char *op, size_t N) {
    if (!bench_op_enabled(o, op)) return;

    int iters  = bench_iters_for(o, op, N);
    int warmup = bench_op_is_heavy(op) && N >= 256 ? (o->warmup > 1 ? 1 : o->warmup)
                                                   : o->warmup;
    char name[96];
    snprintf(name, sizeof(name), "c_%s_%zux%zu", op, N, N);

    if (strcmp(op, "transpose") == 0) {
        CtxTranspose ctx;
        ctx.a = matrix_create(N, N); matrix_fill_rand(&ctx.a, o->seed);
        ctx.out = matrix_create(N, N);
        double t = run_benchmark(bench_transpose, &ctx, warmup, iters, o->repeats);
        bench_report_row(rp, name, N, N, iters, t);
        volatile double sink = ctx.out.data[0]; (void)sink;
        matrix_destroy(&ctx.a); matrix_destroy(&ctx.out);
    } else if (strcmp(op, "add") == 0 || strcmp(op, "sub") == 0) {
        CtxBinary ctx;
        ctx.a = matrix_create(N, N); matrix_fill_rand(&ctx.a, o->seed);
        ctx.b = matrix_create(N, N); matrix_fill_rand(&ctx.b, o->seed + 1u);
        ctx.out = matrix_create(N, N);
        double t = run_benchmark(strcmp(op, "add") == 0 ? bench_add : bench_sub,
                                 &ctx, warmup, iters, o->repeats);
        bench_report_row(rp, name, N, N, iters, t);
        volatile double sink = ctx.out.data[0]; (void)sink;
        matrix_destroy(&ctx.a); matrix_destroy(&ctx.b); matrix_destroy(&ctx.out);
    } else if (strcmp(op, "scale") == 0) {
        CtxScale ctx;
        ctx.a = matrix_create(N, N); matrix_fill_rand(&ctx.a, o->seed);
        ctx.s = 2.5; ctx.out = matrix_create(N, N);
        double t = run_benchmark(bench_scale, &ctx, warmup, iters, o->repeats);
        bench_report_row(rp, name, N, N, iters, t);
        volatile double sink = ctx.out.data[0]; (void)sink;
        matrix_destroy(&ctx.a); matrix_destroy(&ctx.out);
    } else if (strcmp(op, "matvec") == 0) {
        CtxMatVec ctx;
        ctx.a = matrix_create(N, N); matrix_fill_rand(&ctx.a, o->seed);
        ctx.x = (double *)malloc(N * sizeof(double));
        ctx.y = (double *)malloc(N * sizeof(double));
        for (size_t i = 0; i < N; ++i) ctx.x[i] = (double)i * 0.001;
        double t = run_benchmark(bench_matvec, &ctx, warmup, iters, o->repeats);
        bench_report_row(rp, name, N, N, iters, t);
        volatile double sink = ctx.y[0]; (void)sink;
        matrix_destroy(&ctx.a); free(ctx.x); free(ctx.y);
    } else if (strcmp(op, "mul") == 0 || strcmp(op, "transpose_mul") == 0) {
        CtxBinary ctx;
        ctx.a = matrix_create(N, N); matrix_fill_rand(&ctx.a, o->seed);
        ctx.b = matrix_create(N, N); matrix_fill_rand(&ctx.b, o->seed + 1u);
        ctx.out = matrix_create(N, N);
        double t = run_benchmark(strcmp(op, "mul") == 0 ? bench_mul : bench_transpose_mul,
                                 &ctx, warmup, iters, o->repeats);
        bench_report_row(rp, name, N, N, iters, t);
        volatile double sink = ctx.out.data[0]; (void)sink;
        matrix_destroy(&ctx.a); matrix_destroy(&ctx.b); matrix_destroy(&ctx.out);
    } else if (strcmp(op, "add3") == 0) {
        CtxTernary ctx;
        ctx.a = matrix_create(N, N); matrix_fill_rand(&ctx.a, o->seed);
        ctx.b = matrix_create(N, N); matrix_fill_rand(&ctx.b, o->seed + 1u);
        ctx.c = matrix_create(N, N); matrix_fill_rand(&ctx.c, o->seed + 2u);
        ctx.out = matrix_create(N, N);
        double t = run_benchmark(bench_add3, &ctx, warmup, iters, o->repeats);
        bench_report_row(rp, name, N, N, iters, t);
        volatile double sink = ctx.out.data[0]; (void)sink;
        matrix_destroy(&ctx.a); matrix_destroy(&ctx.b);
        matrix_destroy(&ctx.c); matrix_destroy(&ctx.out);
    } else if (strcmp(op, "mul_add") == 0) {
        CtxTernary ctx;
        ctx.a = matrix_create(N, N); matrix_fill_rand(&ctx.a, o->seed);
        ctx.b = matrix_create(N, N); matrix_fill_rand(&ctx.b, o->seed + 1u);
        ctx.c = matrix_create(N, N); matrix_fill_rand(&ctx.c, o->seed + 2u);
        ctx.out = matrix_create(N, N);
        double t = run_benchmark(bench_mul_add, &ctx, warmup, iters, o->repeats);
        bench_report_row(rp, name, N, N, iters, t);
        volatile double sink = ctx.out.data[0]; (void)sink;
        matrix_destroy(&ctx.a); matrix_destroy(&ctx.b);
        matrix_destroy(&ctx.c); matrix_destroy(&ctx.out);
    }
}

int main(int argc, char *argv[]) {
    BenchOptions o;
    if (bench_parse_args(&o, argc, argv) != 0) return 2;
    if (o.help)     { bench_print_usage(argv[0], stdout); return 0; }
    if (o.list_ops) { bench_list_ops(stdout); return 0; }

    /* default CSV path if user did not specify one */
    if (!o.csv_enabled) {
        system("mkdir -p results");
        strncpy(o.csv_path, "results/c_results.csv", BENCH_PATH_LEN - 1);
        o.csv_enabled = 1;
    }

    BenchReport rp;
    bench_report_begin(&rp, &o, "C Matrix Benchmark");

    for (int si = 0; si < o.num_sizes; ++si) {
        for (int oi = 0; oi < BENCH_NUM_ALL_OPS; ++oi)
            run_op(&rp, &o, BENCH_ALL_OPS[oi], o.sizes[si]);
    }

    bench_report_end(&rp, &o);
    return 0;
}
