#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#if defined(_MSC_VER)
#define NOINLINE __declspec(noinline)
#else
#define NOINLINE __attribute__((noinline))
#endif

typedef struct Buffer {
    size_t size;
    uint8_t* data;
} Buffer;

static Buffer buffer_create(size_t size) {
    Buffer b;
    b.size = size;
    b.data = (uint8_t*)malloc(size);
    if (!b.data) {
        fprintf(stderr, "malloc failed\n");
        exit(1);
    }
    for (size_t i = 0; i < size; ++i) {
        b.data[i] = (uint8_t)(i * 131u + 7u);
    }
    return b;
}

static void buffer_destroy(Buffer* b) {
    free(b->data);
    b->data = NULL;
    b->size = 0;
}

static Buffer buffer_deep_copy(const Buffer* src) {
    Buffer dst;
    dst.size = src->size;
    dst.data = (uint8_t*)malloc(dst.size);
    if (!dst.data) {
        fprintf(stderr, "malloc failed\n");
        exit(1);
    }
    memcpy(dst.data, src->data, dst.size);
    return dst;
}

NOINLINE static uint64_t benchmark_copy(size_t size, int rounds) {
    Buffer seed = buffer_create(size);
    uint64_t checksum = 0;

    for (int i = 0; i < rounds; ++i) {
        Buffer copy = buffer_deep_copy(&seed);
        checksum += copy.data[(i * 17) % copy.size];
        buffer_destroy(&copy);
    }

    buffer_destroy(&seed);
    return checksum;
}

static double now_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

int main(int argc, char* argv[]) {
    size_t size = 1u << 20; /* 1 MiB */
    if (argc > 1) {
        size = (size_t)atoll(argv[1]);
        if (size == 0) return 1;
    }
    const int rounds = 2000;

    double t0 = now_sec();
    uint64_t checksum = benchmark_copy(size, rounds);
    double t1 = now_sec();

    printf("c deep-copy measure=%zu time=%.6f sec\n", size, t1 - t0);
    return 0;
}
