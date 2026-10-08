/*
 * "Template" body for the compile-time-sized C kernels.
 *
 * Included once per size by c_matrix/fixed.h with MATRIX_FIXED_N defined to the
 * dimension; deliberately has no include guard.  It is the C stand-in for
 * Eigen::Matrix<double, N, N>: every loop bound is a literal constant, so at
 * -O2 the compiler can fully unroll and keep the data in registers, exactly the
 * lever the C++ side gets from a template parameter.
 *
 * The loop nests (and therefore the per-element summation order) mirror the
 * runtime-sized kernels in ops.c, so results are bit-identical to them.
 */
#define FX_CAT_(a, b) a##b
#define FX_CAT(a, b) FX_CAT_(a, b)
#define FX_TYPE(base) FX_CAT(base, MATRIX_FIXED_N)
#define FX_FN(op) FX_CAT(FX_CAT(fixed, MATRIX_FIXED_N), FX_CAT(_, op))
#define FX_N MATRIX_FIXED_N

typedef struct {
    double m[FX_N][FX_N]; /* row-major */
} FX_TYPE(FixedMat);

typedef struct {
    double v[FX_N];
} FX_TYPE(FixedVec);

static inline void FX_FN(fill_rand)(FX_TYPE(FixedMat) * a, unsigned int seed) {
    unsigned int s = seed;
    for (int i = 0; i < FX_N; ++i)
        for (int j = 0; j < FX_N; ++j) {
            s = s * 1664525u + 1013904223u;
            a->m[i][j] = (double)((int)(s >> 8) % 10000) * 0.0001;
        }
}

static inline void FX_FN(transpose)(const FX_TYPE(FixedMat) * a, FX_TYPE(FixedMat) * out) {
    for (int i = 0; i < FX_N; ++i)
        for (int j = 0; j < FX_N; ++j)
            out->m[j][i] = a->m[i][j];
}

static inline void FX_FN(add)(const FX_TYPE(FixedMat) * a, const FX_TYPE(FixedMat) * b,
                              FX_TYPE(FixedMat) * out) {
    for (int i = 0; i < FX_N; ++i)
        for (int j = 0; j < FX_N; ++j)
            out->m[i][j] = a->m[i][j] + b->m[i][j];
}

static inline void FX_FN(sub)(const FX_TYPE(FixedMat) * a, const FX_TYPE(FixedMat) * b,
                              FX_TYPE(FixedMat) * out) {
    for (int i = 0; i < FX_N; ++i)
        for (int j = 0; j < FX_N; ++j)
            out->m[i][j] = a->m[i][j] - b->m[i][j];
}

static inline void FX_FN(scale)(const FX_TYPE(FixedMat) * a, double s, FX_TYPE(FixedMat) * out) {
    for (int i = 0; i < FX_N; ++i)
        for (int j = 0; j < FX_N; ++j)
            out->m[i][j] = a->m[i][j] * s;
}

static inline void FX_FN(matvec)(const FX_TYPE(FixedMat) * a, const FX_TYPE(FixedVec) * x,
                                 FX_TYPE(FixedVec) * y) {
    for (int i = 0; i < FX_N; ++i) {
        double s = 0.0;
        for (int j = 0; j < FX_N; ++j)
            s += a->m[i][j] * x->v[j];
        y->v[i] = s;
    }
}

static inline void FX_FN(mul)(const FX_TYPE(FixedMat) * a, const FX_TYPE(FixedMat) * b,
                              FX_TYPE(FixedMat) * out) {
    for (int i = 0; i < FX_N; ++i) {
        for (int j = 0; j < FX_N; ++j)
            out->m[i][j] = 0.0;
        for (int k = 0; k < FX_N; ++k) {
            const double aik = a->m[i][k];
            for (int j = 0; j < FX_N; ++j)
                out->m[i][j] += aik * b->m[k][j];
        }
    }
}

static inline void FX_FN(transpose_mul)(const FX_TYPE(FixedMat) * a, const FX_TYPE(FixedMat) * b,
                                        FX_TYPE(FixedMat) * out) {
    for (int i = 0; i < FX_N; ++i)
        for (int j = 0; j < FX_N; ++j)
            out->m[i][j] = 0.0;
    for (int k = 0; k < FX_N; ++k)
        for (int i = 0; i < FX_N; ++i) {
            const double ati = a->m[k][i];
            for (int j = 0; j < FX_N; ++j)
                out->m[i][j] += ati * b->m[k][j];
        }
}

static inline void FX_FN(add3)(const FX_TYPE(FixedMat) * a, const FX_TYPE(FixedMat) * b,
                               const FX_TYPE(FixedMat) * c, FX_TYPE(FixedMat) * out) {
    for (int i = 0; i < FX_N; ++i)
        for (int j = 0; j < FX_N; ++j)
            out->m[i][j] = a->m[i][j] + b->m[i][j] + c->m[i][j];
}

static inline void FX_FN(mul_add)(const FX_TYPE(FixedMat) * a, const FX_TYPE(FixedMat) * b,
                                  const FX_TYPE(FixedMat) * c, FX_TYPE(FixedMat) * out) {
    FX_FN(mul)(a, b, out);
    for (int i = 0; i < FX_N; ++i)
        for (int j = 0; j < FX_N; ++j)
            out->m[i][j] += c->m[i][j];
}

#undef FX_N
#undef FX_FN
#undef FX_TYPE
#undef FX_CAT
#undef FX_CAT_
