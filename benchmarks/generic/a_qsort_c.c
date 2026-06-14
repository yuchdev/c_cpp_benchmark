#define _POSIX_C_SOURCE 199309L
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <time.h>

/*
 * Group "a": qsort callback dispatch vs std::sort + inlined comparator.
 *
 * The element type is intentionally a plain primitive (`double`) with a
 * trivial "less-than" comparison. This isolates the cost of the comparator
 * dispatch itself:
 *   - C qsort can only call the comparator through a function pointer, so it
 *     pays a non-inlinable indirect call on *every* one of the ~N*log2(N)
 *     comparisons.
 *   - C++ std::sort inlines the comparator into the sort body, turning each
 *     comparison into a single machine instruction.
 * With a cheap comparison and a small element, the indirect-call overhead
 * dominates, which is exactly where C++ compile-time optimization shines.
 */

static int cmp_double(const void* a, const void* b) {
    double lhs = *(const double*)a;
    double rhs = *(const double*)b;
    if (lhs < rhs) return -1;
    if (lhs > rhs) return 1;
    return 0;
}

volatile double sink;

double sort_values(double* data, size_t n) {
    qsort(data, n, sizeof(double), cmp_double);

    double sum = 0.0;
    for (size_t i = 0; i < n; ++i) {
        sum += data[i];
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

    double* data = (double*)malloc(n * sizeof(double));
    if (!data) return 1;

    /*
     * Reverse-sorted (descending) input. With a predictable comparison the
     * CPU branch predictor hides the cost of the comparison itself, so what
     * remains is pure dispatch overhead: qsort still pays a non-inlinable
     * indirect call per comparison, while std::sort's inlined comparator is
     * essentially free. This maximizes the C/C++ gap.
     */
    for (size_t i = 0; i < n; ++i) {
        data[i] = (double)(n - i);
    }

    double t0 = now_sec();
    sort_values(data, n);
    double t1 = now_sec();

    printf("c sort measure=%zu time=%.6f sec\n", n, t1 - t0);

    free(data);
    return 0;
}
