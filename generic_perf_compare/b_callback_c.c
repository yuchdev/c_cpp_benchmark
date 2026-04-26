#define _POSIX_C_SOURCE 199309L
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static int transform_value(int x) {
    return x * 3 + 1;
}

void apply_transform(int* out, const int* in, size_t n, int (*op)(int)) {
    for (size_t i = 0; i < n; ++i) {
        out[i] = op(in[i]);
    }
}

volatile int sink;

int run_apply(const int* in, size_t n) {
    int out[64];
    if (n > 64) n = 64;

    apply_transform(out, in, n, transform_value);

    int sum = 0;
    for (size_t i = 0; i < n; ++i) {
        sum += out[i];
    }
    sink = sum;
    return sum;
}

static double now_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

int main(int argc, char* argv[]) {
    int iters = 10000000;
    if (argc > 1) {
        iters = atoi(argv[1]);
        if (iters <= 0) return 1;
    }

    int in[8] = {1,2,3,4,5,6,7,8};
    
    double t0 = now_sec();
    for (int i = 0; i < iters; ++i) {
        run_apply(in, 8);
    }
    double t1 = now_sec();

    printf("c callback measure=%d time=%.6f sec\n", iters, t1 - t0);
    return 0;
}
