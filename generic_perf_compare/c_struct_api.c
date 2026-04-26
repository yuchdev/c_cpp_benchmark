#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

#if defined(_MSC_VER)
#define NOINLINE __declspec(noinline)
#else
#define NOINLINE __attribute__((noinline))
#endif

typedef struct Vec4 {
    float x, y, z, w;
} Vec4;

static inline Vec4 vec_add(const Vec4* a, const Vec4* b) {
    Vec4 r = {a->x + b->x, a->y + b->y, a->z + b->z, a->w + b->w};
    return r;
}

static inline Vec4 vec_scale(const Vec4* a, float s) {
    Vec4 r = {a->x * s, a->y * s, a->z * s, a->w * s};
    return r;
}

static inline float vec_dot(const Vec4* a, const Vec4* b) {
    return a->x * b->x + a->y * b->y + a->z * b->z + a->w * b->w;
}

NOINLINE static float kernel(int iters) {
    Vec4 a = {1.0f, 2.0f, 3.0f, 4.0f};
    Vec4 b = {5.0f, 6.0f, 7.0f, 8.0f};
    Vec4 acc = {0.25f, 0.5f, 0.75f, 1.0f};
    float sink = 0.0f;

    for (int i = 0; i < iters; ++i) {
        Vec4 c = vec_add(&a, &b);
        Vec4 d = vec_scale(&c, 0.00001f * (1.0f + (float)(i & 7)));
        sink += vec_dot(&d, &acc);
        acc = vec_add(&acc, &d);
        Vec4 da = vec_scale(&d, 0.25f);
        Vec4 db = vec_scale(&d, 0.125f);
        a = vec_add(&a, &da);
        b = vec_add(&b, &db);
    }

    volatile float guard = sink + acc.x + acc.y + acc.z + acc.w;
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
    double t0 = now_sec();
    float result = kernel(iters);
    double t1 = now_sec();
    printf("c struct_api measure=%d time=%.6f sec\n", iters, t1 - t0);
    return 0;
}
