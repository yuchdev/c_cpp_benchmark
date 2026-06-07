#define _POSIX_C_SOURCE 199309L
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/*
 * Group "b": function-pointer callback dispatch vs template/lambda transform.
 *
 * Here we transform a large buffer element-by-element. The C version must
 * apply the operation through a *function pointer*. Crucially, the concrete
 * target of that pointer is only known at run time (it is read out of a
 * volatile global), so the compiler cannot devirtualize or inline it. As a
 * result every single element pays the cost of a real, non-inlinable indirect
 * call, and that micro-overhead adds up over the whole buffer.
 *
 * The matching C++ program (b_template_cpp.cpp) passes the same operation as a
 * template argument (a lambda). Because the operation is known at compile time
 * there, the compiler inlines it straight into the loop body and is then free
 * to auto-vectorize the transform. This is exactly the kind of compile-time
 * optimization where C++ pulls ahead of an equivalent C callback.
 */

static int transform_value(int x) {
    return x * 3 + 1;
}

typedef int (*op_t)(int);

/*
 * Volatile global: the optimizer is not allowed to assume which function this
 * points to, so a load of it yields an opaque value. Passing that opaque
 * pointer into apply_transform forces a genuine indirect call per element.
 */
volatile op_t g_op = transform_value;

void apply_transform(int* out, const int* in, size_t n, op_t op) {
    for (size_t i = 0; i < n; ++i) {
        out[i] = op(in[i]);
    }
}

volatile long long sink;

static double now_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

int main(int argc, char* argv[]) {
    size_t n = 10000000;
    if (argc > 1) {
        long v = atol(argv[1]);
        if (v <= 0) return 1;
        n = (size_t)v;
    }

    int* in = (int*)malloc(n * sizeof(int));
    int* out = (int*)malloc(n * sizeof(int));
    if (!in || !out) {
        free(in);
        free(out);
        return 1;
    }

    for (size_t i = 0; i < n; ++i) {
        in[i] = (int)(i & 0xff);
    }

    /* Read the operation through the volatile global: its value is opaque to
     * the optimizer, so the call below cannot be inlined or devirtualized. */
    op_t op = g_op;

    double t0 = now_sec();
    apply_transform(out, in, n, op);
    double t1 = now_sec();

    long long sum = 0;
    for (size_t i = 0; i < n; ++i) {
        sum += out[i];
    }
    sink = sum;

    printf("c callback measure=%zu time=%.6f sec\n", n, t1 - t0);

    free(in);
    free(out);
    return 0;
}
