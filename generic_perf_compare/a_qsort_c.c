#define _POSIX_C_SOURCE 199309L
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <time.h>

typedef struct {
    uint64_t ts;
    uint32_t id;
    uint32_t qty;
} Order;

static int cmp_order(const void* a, const void* b) {
    const Order* lhs = (const Order*)a;
    const Order* rhs = (const Order*)b;
    if (lhs->ts < rhs->ts) return -1;
    if (lhs->ts > rhs->ts) return 1;
    if (lhs->id < rhs->id) return -1;
    if (lhs->id > rhs->id) return 1;
    return 0;
}

volatile uint64_t sink;

uint64_t sort_orders(Order* data, size_t n) {
    qsort(data, n, sizeof(Order), cmp_order);

    uint64_t sum = 0;
    for (size_t i = 0; i < n; ++i) {
        sum += data[i].ts;
        sum += data[i].id;
        sum += data[i].qty;
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
    size_t n = 1000000;
    if (argc > 1) {
        n = (size_t)atoll(argv[1]);
        if (n == 0) return 1;
    }

    Order* data = (Order*)malloc(n * sizeof(Order));
    if (!data) return 1;

    for (size_t i = 0; i < n; ++i) {
        data[i].ts = (uint64_t)(n - i);
        data[i].id = (uint32_t)i;
        data[i].qty = (uint32_t)(i * 10);
    }

    double t0 = now_sec();
    sort_orders(data, n);
    double t1 = now_sec();

    printf("c sort measure=%zu time=%.6f sec\n", n, t1 - t0);

    free(data);
    return 0;
}
