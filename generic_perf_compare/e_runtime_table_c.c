#define _POSIX_C_SOURCE 199309L
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static uint32_t mix32(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

static void init_table(uint32_t table[256]) {
    for (size_t i = 0; i < 256; ++i) {
        table[i] = mix32((uint32_t)i);
    }
}

volatile uint32_t sink;

uint32_t use_table(const uint8_t* data, size_t n) {
    uint32_t table[256];
    init_table(table);

    uint32_t acc = 0;
    for (size_t i = 0; i < n; ++i) {
        acc ^= table[data[i]];
    }
    sink = acc;
    return acc;
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

    uint8_t data[8] = {1, 7, 42, 255, 3, 9, 11, 19};
    
    double t0 = now_sec();
    for (int i = 0; i < iters; ++i) {
        use_table(data, 8);
    }
    double t1 = now_sec();

    printf("c runtime_table measure=%d time=%.6f sec\n", iters, t1 - t0);
    return 0;
}
