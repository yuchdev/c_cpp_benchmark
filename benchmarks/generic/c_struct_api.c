#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

/*
 * Group "c": C struct-style API vs C++ class operators/methods.
 *
 * This benchmark is about *member-access / operation dispatch* overhead.
 *
 * A very common idiom for "objects" in C is a struct of function pointers
 * (a hand-rolled vtable): the data and the operations that act on it are
 * bundled together, and callers invoke an operation by going through the
 * pointer stored in the struct. That is exactly what we model here. The
 * concrete targets are installed at run time and read back through a
 * `volatile` table, so the optimizer must treat them as opaque: every member
 * operation (add / scale / dot) becomes a genuine, non-inlinable indirect
 * call, and that per-operation cost accumulates over the whole loop.
 *
 * The matching C++ program (c_class_operator.cpp) expresses the same data and
 * the same operations as inline operators/methods on a class. Because those
 * operations are known at compile time, the compiler inlines them straight
 * into the loop body and is then free to keep everything in registers and
 * vectorize. This is precisely where C++ compile-time optimization pulls
 * ahead of an equivalent C "struct + function pointer" API.
 */

typedef struct Vec4 {
    float x, y, z, w;
} Vec4;

/* The "methods" of the Vec4 API, implemented as free functions. */
static Vec4 vec_add_impl(const Vec4* a, const Vec4* b) {
    Vec4 r = {a->x + b->x, a->y + b->y, a->z + b->z, a->w + b->w};
    return r;
}

static Vec4 vec_scale_impl(const Vec4* a, float s) {
    Vec4 r = {a->x * s, a->y * s, a->z * s, a->w * s};
    return r;
}

static float vec_dot_impl(const Vec4* a, const Vec4* b) {
    return a->x * b->x + a->y * b->y + a->z * b->z + a->w * b->w;
}

/*
 * The C-style "object": data plus a table of operations. Callers reach an
 * operation only through these pointers.
 */
typedef Vec4 (*add_fn)(const Vec4*, const Vec4*);
typedef Vec4 (*scale_fn)(const Vec4*, float);
typedef float (*dot_fn)(const Vec4*, const Vec4*);

typedef struct Vec4Api {
    add_fn   add;
    scale_fn scale;
    dot_fn   dot;
} Vec4Api;

/*
 * Volatile global API table. Because it is volatile, the optimizer is not
 * allowed to assume which functions these pointers refer to, so each call
 * dispatched through it is a real indirect call that cannot be inlined.
 */
volatile add_fn   g_add   = vec_add_impl;
volatile scale_fn g_scale = vec_scale_impl;
volatile dot_fn   g_dot   = vec_dot_impl;

volatile float sink;

static float kernel(int iters, const Vec4Api* api) {
    Vec4 a = {1.0f, 2.0f, 3.0f, 4.0f};
    Vec4 b = {5.0f, 6.0f, 7.0f, 8.0f};
    Vec4 acc = {0.25f, 0.5f, 0.75f, 1.0f};
    float local = 0.0f;

    for (int i = 0; i < iters; ++i) {
        Vec4 c = api->add(&a, &b);
        Vec4 d = api->scale(&c, 0.00001f * (1.0f + (float)(i & 7)));
        local += api->dot(&d, &acc);
        acc = api->add(&acc, &d);
        Vec4 da = api->scale(&d, 0.25f);
        Vec4 db = api->scale(&d, 0.125f);
        a = api->add(&a, &da);
        b = api->add(&b, &db);
    }

    float guard = local + acc.x + acc.y + acc.z + acc.w;
    sink = guard;
    return guard;
}

static double now_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

int main(int argc, char* argv[]) {
    int iters = 2000000;
    if (argc > 1) {
        iters = atoi(argv[1]);
        if (iters <= 0) return 1;
    }

    /* Build the API table from the opaque volatile pointers. */
    Vec4Api api;
    api.add   = g_add;
    api.scale = g_scale;
    api.dot   = g_dot;

    double t0 = now_sec();
    float result = kernel(iters, &api);
    double t1 = now_sec();
    (void)result;
    printf("c struct_api measure=%d time=%.6f sec\n", iters, t1 - t0);
    return 0;
}
