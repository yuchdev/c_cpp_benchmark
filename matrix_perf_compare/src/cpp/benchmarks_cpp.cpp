/*
 * C++ (Eigen) matrix benchmark driver.
 *
 * Represents the "aggressively optimized modern C++" side.  Built with
 * -O3 -march=native -funroll-loops and full Eigen vectorisation
 * (AVX2 + FMA on this host), it exploits several distinct aspects of
 * matrix performance:
 *
 *   - SIMD element-wise kernels (add / sub / scale / add3)
 *   - vectorised + FMA mat-vec and blocked GEMM (mul / transpose_mul)
 *   - expression-template fusion (add3, mul_add) avoiding temporaries
 *   - compile-time fixed-size matrices (registers, full unrolling)
 *
 * The CLI is shared verbatim with the C driver via bench_options.h.
 */
#include "cpp_matrix/eigen_ops.hpp"
#include "bench_options.h"
#include "bench_report.h"
#include <Eigen/Dense>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <functional>
#include <algorithm>

using Clock = std::chrono::steady_clock;

static double now_ns() {
    return static_cast<double>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            Clock::now().time_since_epoch()).count());
}

template <typename F>
static double run_once(F &&fn, int warmup, int iters) {
    for (int i = 0; i < warmup; ++i) fn();
    double t0 = now_ns();
    for (int i = 0; i < iters; ++i) fn();
    double t1 = now_ns();
    return (t1 - t0) / iters;
}

/* Best-of-repeats average (minimises OS scheduling jitter). */
template <typename F>
static double run_benchmark(F &&fn, int warmup, int iters, int repeats) {
    double best = -1.0;
    for (int r = 0; r < repeats; ++r) {
        double t = run_once(fn, warmup, iters);
        if (best < 0.0 || t < best) best = t;
    }
    return best;
}

// Fill an Eigen matrix using the same LCG as the C version so that the two
// languages operate on identical data.
template <typename Derived>
static void fill_rand(Eigen::MatrixBase<Derived> &m, unsigned int seed) {
    unsigned int s = seed;
    for (int r = 0; r < m.rows(); ++r)
        for (int c = 0; c < m.cols(); ++c) {
            s = s * 1664525u + 1013904223u;
            m(r, c) = static_cast<double>(static_cast<int>(s >> 8) % 10000) * 0.0001;
        }
}

// ---- Group A: dynamic matrices (Eigen::MatrixXd) ----

static void run_dynamic_op(BenchReport &rp, const BenchOptions &o,
                           const std::string &op, int N) {
    if (!bench_op_enabled(&o, op.c_str())) return;

    const int iters  = bench_iters_for(&o, op.c_str(), (size_t)N);
    const int warmup = (bench_op_is_heavy(op.c_str()) && N >= 256)
                       ? std::min(o.warmup, 1) : o.warmup;
    const std::string nm = "cpp_dynamic_" + op + "_" + std::to_string(N) + "x" + std::to_string(N);

    Eigen::MatrixXd A(N, N), B(N, N), C(N, N), Out(N, N);
    Eigen::VectorXd x(N), y(N);
    fill_rand(A, o.seed); fill_rand(B, o.seed + 1u); fill_rand(C, o.seed + 2u);
    for (int i = 0; i < N; ++i) x(i) = i * 0.001;

    double t = 0.0;
    if (op == "transpose")
        t = run_benchmark([&]{ Out.noalias() = A.transpose(); }, warmup, iters, o.repeats);
    else if (op == "add")
        t = run_benchmark([&]{ Out.noalias() = A + B; }, warmup, iters, o.repeats);
    else if (op == "sub")
        t = run_benchmark([&]{ Out.noalias() = A - B; }, warmup, iters, o.repeats);
    else if (op == "scale")
        t = run_benchmark([&]{ Out.noalias() = A * 2.5; }, warmup, iters, o.repeats);
    else if (op == "matvec")
        t = run_benchmark([&]{ y.noalias() = A * x; }, warmup, iters, o.repeats);
    else if (op == "mul")
        t = run_benchmark([&]{ Out.noalias() = A * B; }, warmup, iters, o.repeats);
    else if (op == "transpose_mul")
        t = run_benchmark([&]{ Out.noalias() = A.transpose() * B; }, warmup, iters, o.repeats);
    else if (op == "add3")
        t = run_benchmark([&]{ Out.noalias() = A + B + C; }, warmup, iters, o.repeats);
    else if (op == "mul_add")
        t = run_benchmark([&]{ Out.noalias() = A * B + C; }, warmup, iters, o.repeats);
    else
        return;

    bench_report_row(&rp, nm.c_str(), N, N, iters, t);
    volatile double sink = Out(0, 0) + y(0); (void)sink;
}

// ---- Group B: fixed-size matrices (compile-time N) ----

template <int N>
static void run_fixed(BenchReport &rp, const BenchOptions &o) {
    using Mat = Eigen::Matrix<double, N, N>;
    using Vec = Eigen::Matrix<double, N, 1>;

    Mat A, B, C, Out;
    Vec x, y;
    fill_rand(A, o.seed); fill_rand(B, o.seed + 1u); fill_rand(C, o.seed + 2u);
    for (int i = 0; i < N; ++i) x(i) = i * 0.001;

    const std::string sz = std::to_string(N) + "x" + std::to_string(N);
    const int it = o.iters;

    auto emit = [&](const char *op, double t) {
        std::string nm = std::string("cpp_fixed_") + op + "_" + sz;
        bench_report_row(&rp, nm.c_str(), N, N, it, t);
    };

    if (bench_op_enabled(&o, "transpose"))
        emit("transpose", run_benchmark([&]{ Out.noalias() = A.transpose(); }, o.warmup, it, o.repeats));
    if (bench_op_enabled(&o, "add"))
        emit("add", run_benchmark([&]{ Out.noalias() = A + B; }, o.warmup, it, o.repeats));
    if (bench_op_enabled(&o, "sub"))
        emit("sub", run_benchmark([&]{ Out.noalias() = A - B; }, o.warmup, it, o.repeats));
    if (bench_op_enabled(&o, "scale"))
        emit("scale", run_benchmark([&]{ Out.noalias() = A * 2.5; }, o.warmup, it, o.repeats));
    if (bench_op_enabled(&o, "matvec"))
        emit("matvec", run_benchmark([&]{ y.noalias() = A * x; }, o.warmup, it, o.repeats));
    if (bench_op_enabled(&o, "mul"))
        emit("mul", run_benchmark([&]{ Out.noalias() = A * B; }, o.warmup, it, o.repeats));
    if (bench_op_enabled(&o, "transpose_mul"))
        emit("transpose_mul", run_benchmark([&]{ Out.noalias() = A.transpose() * B; }, o.warmup, it, o.repeats));
    if (bench_op_enabled(&o, "add3"))
        emit("add3", run_benchmark([&]{ Out.noalias() = A + B + C; }, o.warmup, it, o.repeats));
    if (bench_op_enabled(&o, "mul_add"))
        emit("mul_add", run_benchmark([&]{ Out.noalias() = A * B + C; }, o.warmup, it, o.repeats));

    volatile double sink = Out(0, 0) + y(0); (void)sink;
}

int main(int argc, char *argv[]) {
    BenchOptions o;
    if (bench_parse_args(&o, argc, argv) != 0) return 2;
    if (o.help)     { bench_print_usage(argv[0], stdout); return 0; }
    if (o.list_ops) { bench_list_ops(stdout); return 0; }

    if (!o.csv_enabled) {
        std::system("mkdir -p results");
        std::strncpy(o.csv_path, "results/cpp_results.csv", BENCH_PATH_LEN - 1);
        o.csv_enabled = 1;
    }

    BenchReport rp;
    bench_report_begin(&rp, &o, "C++ (Eigen) Matrix Benchmark");

    bench_report_section(&rp, "Group A: Dynamic matrices");
    for (int si = 0; si < o.num_sizes; ++si)
        for (int oi = 0; oi < BENCH_NUM_ALL_OPS; ++oi)
            run_dynamic_op(rp, o, BENCH_ALL_OPS[oi], (int)o.sizes[si]);

    if (o.fixed_enabled) {
        bench_report_section(&rp, "Group B: Fixed-size matrices");
        run_fixed<3>(rp, o);
        run_fixed<4>(rp, o);
        run_fixed<8>(rp, o);
        run_fixed<16>(rp, o);
    }

    bench_report_end(&rp, &o);
    return 0;
}
