#include "c_matrix/matrix.h"
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

static double run_benchmark(bench_fn fn, void *ctx, int warmup, int iters) {
    for (int i = 0; i < warmup; ++i) fn(ctx);
    double t0 = now_ns();
    for (int i = 0; i < iters; ++i) fn(ctx);
    double t1 = now_ns();
    return (t1 - t0) / iters;
}

/* ---- per-operation contexts ---- */
typedef struct { Matrix a; Matrix out; } CtxTranspose;
typedef struct { Matrix a, b, out; } CtxBinary;
typedef struct { Matrix a; double s; Matrix out; } CtxScale;
typedef struct { Matrix a; double *x; double *y; } CtxMatVec;
typedef struct { Matrix a, b, c, out; } CtxTernary;

static void bench_transpose(void *p) {
    CtxTranspose *c = (CtxTranspose *)p;
    matrix_transpose(&c->a, &c->out);
}
static void bench_add(void *p) {
    CtxBinary *c = (CtxBinary *)p;
    matrix_add(&c->a, &c->b, &c->out);
}
static void bench_sub(void *p) {
    CtxBinary *c = (CtxBinary *)p;
    matrix_sub(&c->a, &c->b, &c->out);
}
static void bench_scale(void *p) {
    CtxScale *c = (CtxScale *)p;
    matrix_scale(&c->a, c->s, &c->out);
}
static void bench_matvec(void *p) {
    CtxMatVec *c = (CtxMatVec *)p;
    matrix_matvec(&c->a, c->x, c->y);
}
static void bench_mul(void *p) {
    CtxBinary *c = (CtxBinary *)p;
    matrix_mul(&c->a, &c->b, &c->out);
}
static void bench_transpose_mul(void *p) {
    CtxBinary *c = (CtxBinary *)p;
    matrix_transpose_mul(&c->a, &c->b, &c->out);
}
static void bench_add3(void *p) {
    CtxTernary *c = (CtxTernary *)p;
    matrix_add3(&c->a, &c->b, &c->c, &c->out);
}
static void bench_mul_add(void *p) {
    CtxTernary *c = (CtxTernary *)p;
    matrix_mul_add(&c->a, &c->b, &c->c, &c->out);
}

/* ---- reporting ---- */
static FILE *g_csv = NULL;
static void report(const char *name, size_t rows, size_t cols, int iters, double avg_ns) {
    printf("%-45s  %4zux%-4zu  iters=%3d  avg=%10.1f ns\n",
           name, rows, cols, iters, avg_ns);
    if (g_csv)
        fprintf(g_csv, "%s,%zu,%zu,%d,%.1f\n", name, rows, cols, iters, avg_ns);
}

/* ---- run one size ---- */
static void run_size(size_t N, int warmup, int iters) {
    /* transpose */
    {
        CtxTranspose ctx;
        ctx.a   = matrix_create(N, N); matrix_fill_rand(&ctx.a,   1);
        ctx.out = matrix_create(N, N);
        char name[64]; snprintf(name, sizeof(name), "c_transpose_%zux%zu", N, N);
        double t = run_benchmark(bench_transpose, &ctx, warmup, iters);
        report(name, N, N, iters, t);
        /* consume */
        volatile double sink = ctx.out.data[0]; (void)sink;
        matrix_destroy(&ctx.a); matrix_destroy(&ctx.out);
    }
    /* add */
    {
        CtxBinary ctx;
        ctx.a   = matrix_create(N, N); matrix_fill_rand(&ctx.a,   1);
        ctx.b   = matrix_create(N, N); matrix_fill_rand(&ctx.b,   2);
        ctx.out = matrix_create(N, N);
        char name[64]; snprintf(name, sizeof(name), "c_add_%zux%zu", N, N);
        double t = run_benchmark(bench_add, &ctx, warmup, iters);
        report(name, N, N, iters, t);
        volatile double sink = ctx.out.data[0]; (void)sink;
        matrix_destroy(&ctx.a); matrix_destroy(&ctx.b); matrix_destroy(&ctx.out);
    }
    /* sub */
    {
        CtxBinary ctx;
        ctx.a   = matrix_create(N, N); matrix_fill_rand(&ctx.a,   1);
        ctx.b   = matrix_create(N, N); matrix_fill_rand(&ctx.b,   2);
        ctx.out = matrix_create(N, N);
        char name[64]; snprintf(name, sizeof(name), "c_sub_%zux%zu", N, N);
        double t = run_benchmark(bench_sub, &ctx, warmup, iters);
        report(name, N, N, iters, t);
        volatile double sink = ctx.out.data[0]; (void)sink;
        matrix_destroy(&ctx.a); matrix_destroy(&ctx.b); matrix_destroy(&ctx.out);
    }
    /* scale */
    {
        CtxScale ctx;
        ctx.a   = matrix_create(N, N); matrix_fill_rand(&ctx.a,   1);
        ctx.s   = 2.5;
        ctx.out = matrix_create(N, N);
        char name[64]; snprintf(name, sizeof(name), "c_scale_%zux%zu", N, N);
        double t = run_benchmark(bench_scale, &ctx, warmup, iters);
        report(name, N, N, iters, t);
        volatile double sink = ctx.out.data[0]; (void)sink;
        matrix_destroy(&ctx.a); matrix_destroy(&ctx.out);
    }
    /* matvec */
    {
        CtxMatVec ctx;
        ctx.a = matrix_create(N, N); matrix_fill_rand(&ctx.a, 1);
        ctx.x = (double *)malloc(N * sizeof(double));
        ctx.y = (double *)malloc(N * sizeof(double));
        for (size_t i = 0; i < N; ++i) ctx.x[i] = (double)i * 0.001;
        char name[64]; snprintf(name, sizeof(name), "c_matvec_%zux%zu", N, N);
        double t = run_benchmark(bench_matvec, &ctx, warmup, iters);
        report(name, N, N, iters, t);
        volatile double sink = ctx.y[0]; (void)sink;
        matrix_destroy(&ctx.a); free(ctx.x); free(ctx.y);
    }
    /* mul - fewer iterations for large N */
    int mul_iters = (N >= 256) ? (iters / 4 < 1 ? 1 : iters / 4) : iters;
    {
        CtxBinary ctx;
        ctx.a   = matrix_create(N, N); matrix_fill_rand(&ctx.a,   1);
        ctx.b   = matrix_create(N, N); matrix_fill_rand(&ctx.b,   2);
        ctx.out = matrix_create(N, N);
        char name[64]; snprintf(name, sizeof(name), "c_mul_%zux%zu", N, N);
        double t = run_benchmark(bench_mul, &ctx, 1, mul_iters);
        report(name, N, N, mul_iters, t);
        volatile double sink = ctx.out.data[0]; (void)sink;
        matrix_destroy(&ctx.a); matrix_destroy(&ctx.b); matrix_destroy(&ctx.out);
    }
    /* transpose_mul */
    {
        CtxBinary ctx;
        ctx.a   = matrix_create(N, N); matrix_fill_rand(&ctx.a,   1);
        ctx.b   = matrix_create(N, N); matrix_fill_rand(&ctx.b,   2);
        ctx.out = matrix_create(N, N);
        char name[64]; snprintf(name, sizeof(name), "c_transpose_mul_%zux%zu", N, N);
        double t = run_benchmark(bench_transpose_mul, &ctx, 1, mul_iters);
        report(name, N, N, mul_iters, t);
        volatile double sink = ctx.out.data[0]; (void)sink;
        matrix_destroy(&ctx.a); matrix_destroy(&ctx.b); matrix_destroy(&ctx.out);
    }
    /* add3 */
    {
        CtxTernary ctx;
        ctx.a   = matrix_create(N, N); matrix_fill_rand(&ctx.a, 1);
        ctx.b   = matrix_create(N, N); matrix_fill_rand(&ctx.b, 2);
        ctx.c   = matrix_create(N, N); matrix_fill_rand(&ctx.c, 3);
        ctx.out = matrix_create(N, N);
        char name[64]; snprintf(name, sizeof(name), "c_add3_%zux%zu", N, N);
        double t = run_benchmark(bench_add3, &ctx, warmup, iters);
        report(name, N, N, iters, t);
        volatile double sink = ctx.out.data[0]; (void)sink;
        matrix_destroy(&ctx.a); matrix_destroy(&ctx.b);
        matrix_destroy(&ctx.c); matrix_destroy(&ctx.out);
    }
    /* mul_add */
    {
        CtxTernary ctx;
        ctx.a   = matrix_create(N, N); matrix_fill_rand(&ctx.a, 1);
        ctx.b   = matrix_create(N, N); matrix_fill_rand(&ctx.b, 2);
        ctx.c   = matrix_create(N, N); matrix_fill_rand(&ctx.c, 3);
        ctx.out = matrix_create(N, N);
        char name[64]; snprintf(name, sizeof(name), "c_mul_add_%zux%zu", N, N);
        double t = run_benchmark(bench_mul_add, &ctx, 1, mul_iters);
        report(name, N, N, mul_iters, t);
        volatile double sink = ctx.out.data[0]; (void)sink;
        matrix_destroy(&ctx.a); matrix_destroy(&ctx.b);
        matrix_destroy(&ctx.c); matrix_destroy(&ctx.out);
    }
}

int main(int argc, char *argv[]) {
    const char *csv_path = (argc > 1) ? argv[1] : "results/c_results.csv";

    /* Ensure results directory exists */
    system("mkdir -p results");

    g_csv = fopen(csv_path, "w");
    if (g_csv)
        fprintf(g_csv, "name,rows,cols,iterations,avg_ns\n");
    else
        fprintf(stderr, "Warning: could not open %s for writing\n", csv_path);

    printf("=== C Matrix Benchmark ===\n\n");
    printf("%-45s  %9s  %8s  %14s\n", "Benchmark", "Size", "Iters", "Avg Time");
    printf("%-45s  %9s  %8s  %14s\n",
           "---------------------------------------------",
           "---------", "--------", "--------------");

    int warmup = 3, iters = 20;

    run_size(32,  warmup, iters);
    printf("\n");
    run_size(128, warmup, iters);
    printf("\n");
    run_size(512, warmup, iters / 2);

    if (g_csv) fclose(g_csv);
    printf("\nResults written to %s\n", csv_path);
    return 0;
}
