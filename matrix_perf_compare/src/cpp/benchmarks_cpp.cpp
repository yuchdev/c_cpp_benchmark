#include "cpp_matrix/eigen_ops.hpp"
#include <Eigen/Dense>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <functional>
#include <vector>

using Clock = std::chrono::high_resolution_clock;

static FILE *g_csv = nullptr;

static double now_ns() {
    return static_cast<double>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            Clock::now().time_since_epoch()).count());
}

static double run_benchmark(std::function<void()> fn, int warmup, int iters) {
    for (int i = 0; i < warmup; ++i) fn();
    double t0 = now_ns();
    for (int i = 0; i < iters; ++i) fn();
    double t1 = now_ns();
    return (t1 - t0) / iters;
}

static void report(const char *name, int rows, int cols, int iters, double avg_ns) {
    std::printf("%-55s  %4dx%-4d  iters=%3d  avg=%10.1f ns\n",
                name, rows, cols, iters, avg_ns);
    if (g_csv)
        std::fprintf(g_csv, "%s,%d,%d,%d,%.1f\n", name, rows, cols, iters, avg_ns);
}

// Fill dynamic Eigen matrix with same LCG as C version
static void fill_rand(Eigen::MatrixXd &m, unsigned int seed) {
    unsigned int s = seed;
    for (int r = 0; r < m.rows(); ++r)
        for (int c = 0; c < m.cols(); ++c) {
            s = s * 1664525u + 1013904223u;
            m(r, c) = static_cast<double>(static_cast<int>(s >> 8) % 10000) * 0.0001;
        }
}

// ---- Group A: Dynamic matrices ----

static void bench_dynamic(int N, int warmup, int iters) {
    Eigen::MatrixXd A(N, N), B(N, N), C(N, N), D(N, N), Out(N, N);
    Eigen::VectorXd x(N), y(N);
    fill_rand(A, 1); fill_rand(B, 2); fill_rand(C, 3); fill_rand(D, 4);
    for (int i = 0; i < N; ++i) x(i) = i * 0.001;

    int mul_iters = (N >= 256) ? std::max(1, iters / 4) : iters;

    // transpose
    {
        std::string nm = "cpp_dynamic_transpose_" + std::to_string(N) + "x" + std::to_string(N);
        double t = run_benchmark([&]{ Out.noalias() = A.transpose(); }, warmup, iters);
        report(nm.c_str(), N, N, iters, t);
        volatile double sink = Out(0,0); (void)sink;
    }
    // add
    {
        std::string nm = "cpp_dynamic_add_" + std::to_string(N) + "x" + std::to_string(N);
        double t = run_benchmark([&]{ Out.noalias() = A + B; }, warmup, iters);
        report(nm.c_str(), N, N, iters, t);
        volatile double sink = Out(0,0); (void)sink;
    }
    // sub
    {
        std::string nm = "cpp_dynamic_sub_" + std::to_string(N) + "x" + std::to_string(N);
        double t = run_benchmark([&]{ Out.noalias() = A - B; }, warmup, iters);
        report(nm.c_str(), N, N, iters, t);
        volatile double sink = Out(0,0); (void)sink;
    }
    // scale
    {
        std::string nm = "cpp_dynamic_scale_" + std::to_string(N) + "x" + std::to_string(N);
        double t = run_benchmark([&]{ Out.noalias() = A * 2.5; }, warmup, iters);
        report(nm.c_str(), N, N, iters, t);
        volatile double sink = Out(0,0); (void)sink;
    }
    // matvec
    {
        std::string nm = "cpp_dynamic_matvec_" + std::to_string(N) + "x" + std::to_string(N);
        double t = run_benchmark([&]{ y.noalias() = A * x; }, warmup, iters);
        report(nm.c_str(), N, N, iters, t);
        volatile double sink = y(0); (void)sink;
    }
    // mul
    {
        std::string nm = "cpp_dynamic_mul_" + std::to_string(N) + "x" + std::to_string(N);
        double t = run_benchmark([&]{ Out.noalias() = A * B; }, 1, mul_iters);
        report(nm.c_str(), N, N, mul_iters, t);
        volatile double sink = Out(0,0); (void)sink;
    }
    // transpose_mul  A^T * B
    {
        std::string nm = "cpp_dynamic_transpose_mul_" + std::to_string(N) + "x" + std::to_string(N);
        double t = run_benchmark([&]{ Out.noalias() = A.transpose() * B; }, 1, mul_iters);
        report(nm.c_str(), N, N, mul_iters, t);
        volatile double sink = Out(0,0); (void)sink;
    }
    // add3  A + B + C  - Eigen fuses into single pass via expression templates
    {
        std::string nm = "cpp_dynamic_add3_" + std::to_string(N) + "x" + std::to_string(N);
        double t = run_benchmark([&]{ Out = A + B + C; }, warmup, iters);
        report(nm.c_str(), N, N, iters, t);
        volatile double sink = Out(0,0); (void)sink;
    }
    // mul_add  A*B + C  - Eigen .noalias() avoids extra temporary
    {
        std::string nm = "cpp_dynamic_mul_add_" + std::to_string(N) + "x" + std::to_string(N);
        double t = run_benchmark([&]{ Out.noalias() = A * B + C; }, 1, mul_iters);
        report(nm.c_str(), N, N, mul_iters, t);
        volatile double sink = Out(0,0); (void)sink;
    }
}

// ---- Group B: Fixed-size matrices ----

template <int N>
static void bench_fixed(int warmup, int iters) {
    using Mat = Eigen::Matrix<double, N, N>;
    using Vec = Eigen::Matrix<double, N, 1>;

    Mat A, B, C, Out;
    Vec x, y;
    // fill with LCG
    {
        unsigned int s = 1;
        for (int r = 0; r < N; ++r) for (int c = 0; c < N; ++c) {
            s = s * 1664525u + 1013904223u;
            A(r,c) = static_cast<double>(static_cast<int>(s >> 8) % 10000) * 0.0001;
        }
        s = 2;
        for (int r = 0; r < N; ++r) for (int c = 0; c < N; ++c) {
            s = s * 1664525u + 1013904223u;
            B(r,c) = static_cast<double>(static_cast<int>(s >> 8) % 10000) * 0.0001;
        }
        s = 3;
        for (int r = 0; r < N; ++r) for (int c = 0; c < N; ++c) {
            s = s * 1664525u + 1013904223u;
            C(r,c) = static_cast<double>(static_cast<int>(s >> 8) % 10000) * 0.0001;
        }
        for (int i = 0; i < N; ++i) x(i) = i * 0.001;
    }

    std::string sz = std::to_string(N) + "x" + std::to_string(N);

    // transpose
    {
        std::string nm = "cpp_fixed_transpose_" + sz;
        double t = run_benchmark([&]{ Out = A.transpose(); }, warmup, iters);
        report(nm.c_str(), N, N, iters, t);
        volatile double sink = Out(0,0); (void)sink;
    }
    // add
    {
        std::string nm = "cpp_fixed_add_" + sz;
        double t = run_benchmark([&]{ Out = A + B; }, warmup, iters);
        report(nm.c_str(), N, N, iters, t);
        volatile double sink = Out(0,0); (void)sink;
    }
    // mul
    {
        std::string nm = "cpp_fixed_mul_" + sz;
        double t = run_benchmark([&]{ Out.noalias() = A * B; }, warmup, iters);
        report(nm.c_str(), N, N, iters, t);
        volatile double sink = Out(0,0); (void)sink;
    }
    // matvec
    {
        std::string nm = "cpp_fixed_matvec_" + sz;
        double t = run_benchmark([&]{ y.noalias() = A * x; }, warmup, iters);
        report(nm.c_str(), N, N, iters, t);
        volatile double sink = y(0); (void)sink;
    }
    // add3  (expression template fusion)
    {
        std::string nm = "cpp_fixed_add3_" + sz;
        double t = run_benchmark([&]{ Out = A + B + C; }, warmup, iters);
        report(nm.c_str(), N, N, iters, t);
        volatile double sink = Out(0,0); (void)sink;
    }
    // transpose_mul
    {
        std::string nm = "cpp_fixed_transpose_mul_" + sz;
        double t = run_benchmark([&]{ Out.noalias() = A.transpose() * B; }, warmup, iters);
        report(nm.c_str(), N, N, iters, t);
        volatile double sink = Out(0,0); (void)sink;
    }
    // mul_add  A*B + C
    {
        std::string nm = "cpp_fixed_mul_add_" + sz;
        double t = run_benchmark([&]{ Out.noalias() = A * B + C; }, warmup, iters);
        report(nm.c_str(), N, N, iters, t);
        volatile double sink = Out(0,0); (void)sink;
    }
}

// ---- Group C: Expression template / fusion demo ----

static void bench_expr_fusion(int warmup, int iters) {
    int N = 128;
    Eigen::MatrixXd A(N,N), B(N,N), C(N,N), Out(N,N);
    fill_rand(A, 1); fill_rand(B, 2); fill_rand(C, 3);

    // C = A^T * B  - fused: no temporary for transpose
    {
        double t = run_benchmark([&]{ Out.noalias() = A.transpose() * B; }, warmup, iters);
        report("cpp_expr_AT_mul_B_128x128", N, N, iters, t);
        volatile double sink = Out(0,0); (void)sink;
    }
    // C = A*B + D  - .noalias avoids extra copy
    {
        double t = run_benchmark([&]{ Out.noalias() = A * B + C; }, warmup, iters);
        report("cpp_expr_A_mul_B_add_C_128x128", N, N, iters, t);
        volatile double sink = Out(0,0); (void)sink;
    }
    // G = A + B + C  - single traversal via expression templates
    {
        double t = run_benchmark([&]{ Out = A + B + C; }, warmup, iters);
        report("cpp_expr_A_add_B_add_C_128x128", N, N, iters, t);
        volatile double sink = Out(0,0); (void)sink;
    }
}

int main(int argc, char *argv[]) {
    const char *csv_path = (argc > 1) ? argv[1] : "results/cpp_results.csv";

    system("mkdir -p results");

    g_csv = std::fopen(csv_path, "w");
    if (g_csv)
        std::fprintf(g_csv, "name,rows,cols,iterations,avg_ns\n");
    else
        std::fprintf(stderr, "Warning: could not open %s for writing\n", csv_path);

    std::printf("=== C++ (Eigen) Matrix Benchmark ===\n\n");
    std::printf("%-55s  %9s  %8s  %14s\n", "Benchmark", "Size", "Iters", "Avg Time");
    std::printf("%-55s  %9s  %8s  %14s\n",
                "-------------------------------------------------------",
                "---------", "--------", "--------------");

    int warmup = 3, iters = 20;

    std::printf("\n--- Group A: Dynamic matrices ---\n");
    bench_dynamic(32,  warmup, iters);
    std::printf("\n");
    bench_dynamic(128, warmup, iters);
    std::printf("\n");
    bench_dynamic(512, warmup, iters / 2);

    std::printf("\n--- Group B: Fixed-size matrices ---\n");
    bench_fixed<3>(warmup, iters);
    bench_fixed<4>(warmup, iters);
    bench_fixed<8>(warmup, iters);
    bench_fixed<16>(warmup, iters);

    std::printf("\n--- Group C: Expression template fusion ---\n");
    bench_expr_fusion(warmup, 5);

    if (g_csv) std::fclose(g_csv);
    std::printf("\nResults written to %s\n", csv_path);
    return 0;
}
