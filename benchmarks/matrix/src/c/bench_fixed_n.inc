/*
 * Per-size body of the C fixed-size sweep.
 *
 * Included once per N by bench_scenarios_c.c with BENCH_FIXED_RUN_N defined
 * (no include guard on purpose).  Instantiates bench_fixed_run_<N>(), which times
 * all nine ops on compile-time N x N matrices using the fixedN_<op>() kernels from
 * c_matrix/fixed.h -- the C twin of Fixed<N> in cpp_matrix/eigen_scenarios.hpp.
 */
#define FXB_CAT_(a, b) a##b
#define FXB_CAT(a, b) FXB_CAT_(a, b)
#define FXB_N BENCH_FIXED_RUN_N
#define FXB_TYPE(base) FXB_CAT(base, BENCH_FIXED_RUN_N)
#define FXB_FN(op) FXB_CAT(FXB_CAT(fixed, BENCH_FIXED_RUN_N), FXB_CAT(_, op))

/* Time `call` as op `opname` and emit the row (skipped when --ops excludes it). */
#define FXB_RUN_OP(opname, ...)                                                                \
    do {                                                                                       \
        if (bench_op_enabled(o, (opname))) {                                                   \
            const double work_ = bench_work_op((opname), (double)FXB_N);                       \
            const int iters_ = bench_iters_scaled(o, work_);                                   \
            const int warmup_ = bench_warmup_for(o, work_);                                    \
            double t_;                                                                         \
            BENCH_MEASURE(t_, warmup_, iters_, o->repeats, __VA_ARGS__);                       \
            char name_[96];                                                                    \
            bench_scn_name(name_, sizeof(name_), "c", "fixed", (opname), -1, FXB_N, FXB_N);    \
            bench_report_row(rp, name_, FXB_N, FXB_N, iters_, t_);                             \
        }                                                                                      \
    } while (0)

static void FXB_CAT(bench_fixed_run_, BENCH_FIXED_RUN_N)(BenchReport *rp, const BenchOptions *o) {
    FXB_TYPE(FixedMat) a, b, c, out;
    FXB_TYPE(FixedVec) x, y;
    FXB_FN(fill_rand)(&a, o->seed);
    FXB_FN(fill_rand)(&b, o->seed + 1u);
    FXB_FN(fill_rand)(&c, o->seed + 2u);
    memset(&out, 0, sizeof(out));
    memset(&y, 0, sizeof(y));
    for (int i = 0; i < FXB_N; ++i)
        x.v[i] = i * 0.001;
    BENCH_ESCAPE(&a); BENCH_ESCAPE(&b); BENCH_ESCAPE(&c);
    BENCH_ESCAPE(&out); BENCH_ESCAPE(&x); BENCH_ESCAPE(&y);

    FXB_RUN_OP("transpose", FXB_FN(transpose)(&a, &out));
    FXB_RUN_OP("add", FXB_FN(add)(&a, &b, &out));
    FXB_RUN_OP("sub", FXB_FN(sub)(&a, &b, &out));
    FXB_RUN_OP("scale", FXB_FN(scale)(&a, 2.5, &out));
    FXB_RUN_OP("matvec", FXB_FN(matvec)(&a, &x, &y));
    FXB_RUN_OP("mul", FXB_FN(mul)(&a, &b, &out));
    FXB_RUN_OP("transpose_mul", FXB_FN(transpose_mul)(&a, &b, &out));
    FXB_RUN_OP("add3", FXB_FN(add3)(&a, &b, &c, &out));
    FXB_RUN_OP("mul_add", FXB_FN(mul_add)(&a, &b, &c, &out));

    volatile double sink = out.m[0][0] + y.v[0];
    (void)sink;
}

#undef FXB_RUN_OP
#undef FXB_FN
#undef FXB_TYPE
#undef FXB_N
#undef FXB_CAT
#undef FXB_CAT_
