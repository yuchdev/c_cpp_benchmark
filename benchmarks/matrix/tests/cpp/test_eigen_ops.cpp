/*
 * C++ / Eigen correctness tests.
 * Compares C and Eigen results within tolerance, and verifies Eigen-only results.
 */
#include "cpp_matrix/eigen_ops.hpp"
#include "c_matrix/matrix.h"
#include <Eigen/Dense>
#include <cstdio>
#include <cmath>
#include <cstring>

static int g_pass = 0, g_fail = 0;

static void check(const char *name, bool cond) {
    if (cond) { std::printf("[PASS] %s\n", name); ++g_pass; }
    else       { std::printf("[FAIL] %s\n", name); ++g_fail; }
}

static const double TOL = 1e-9;

// Fill Eigen dynamic matrix with same LCG as C version
static void fill_rand(Eigen::MatrixXd &m, unsigned int seed) {
    unsigned int s = seed;
    for (int r = 0; r < m.rows(); ++r)
        for (int c = 0; c < m.cols(); ++c) {
            s = s * 1664525u + 1013904223u;
            m(r,c) = static_cast<double>(static_cast<int>(s >> 8) % 10000) * 0.0001;
        }
}

// Convert Eigen matrix to C Matrix for cross-comparison
static Matrix eigen_to_c(const Eigen::MatrixXd &e) {
    Matrix m = matrix_create(static_cast<size_t>(e.rows()), static_cast<size_t>(e.cols()));
    for (int r = 0; r < e.rows(); ++r)
        for (int c = 0; c < e.cols(); ++c)
            matrix_set(&m, static_cast<size_t>(r), static_cast<size_t>(c), e(r,c));
    return m;
}

static bool matrices_equal(const Matrix *a, const Matrix *b, double tol) {
    if (a->rows != b->rows || a->cols != b->cols) return false;
    size_t n = a->rows * a->cols;
    for (size_t i = 0; i < n; ++i)
        if (std::fabs(a->data[i] - b->data[i]) > tol) return false;
    return true;
}

static void test_transpose_vs_c(int N) {
    Eigen::MatrixXd A(N,N);
    fill_rand(A, 1);

    // C++: compute transpose of A
    Eigen::MatrixXd Et = A.transpose();
    Matrix et = eigen_to_c(Et);

    // C: populate from the same A (already filled above), then transpose
    Matrix ca = matrix_create(N,N);
    for (int r = 0; r < N; ++r) for (int c = 0; c < N; ++c)
        matrix_set(&ca, r, c, A(r,c));
    Matrix ct = matrix_create(N,N);
    matrix_transpose(&ca, &ct);

    char name[64]; std::snprintf(name, sizeof(name), "transpose_match_%dx%d", N, N);
    check(name, matrices_equal(&et, &ct, TOL));

    matrix_destroy(&ca); matrix_destroy(&ct); matrix_destroy(&et);
}

static void test_mul_vs_c(int N) {
    Eigen::MatrixXd A(N,N), B(N,N);
    fill_rand(A,1); fill_rand(B,2);

    Eigen::MatrixXd ER(N,N);
    ER.noalias() = A * B;
    Matrix er = eigen_to_c(ER);

    Matrix ca = matrix_create(N,N), cb = matrix_create(N,N), cr = matrix_create(N,N);
    for (int r = 0; r < N; ++r) for (int c = 0; c < N; ++c) {
        matrix_set(&ca,r,c,A(r,c));
        matrix_set(&cb,r,c,B(r,c));
    }
    matrix_mul(&ca, &cb, &cr);

    char name[64]; std::snprintf(name, sizeof(name), "mul_match_%dx%d", N, N);
    // Use slightly larger tolerance for larger matrices due to floating-point reordering
    check(name, matrices_equal(&er, &cr, 1e-6));

    matrix_destroy(&ca); matrix_destroy(&cb); matrix_destroy(&cr); matrix_destroy(&er);
}

static void test_add_vs_c(int N) {
    Eigen::MatrixXd A(N,N), B(N,N);
    fill_rand(A,1); fill_rand(B,2);

    Eigen::MatrixXd ER = A + B;
    Matrix er = eigen_to_c(ER);

    Matrix ca = matrix_create(N,N), cb = matrix_create(N,N), cr = matrix_create(N,N);
    for (int r = 0; r < N; ++r) for (int c = 0; c < N; ++c) {
        matrix_set(&ca,r,c,A(r,c));
        matrix_set(&cb,r,c,B(r,c));
    }
    matrix_add(&ca, &cb, &cr);

    char name[64]; std::snprintf(name, sizeof(name), "add_match_%dx%d", N, N);
    check(name, matrices_equal(&er, &cr, TOL));

    matrix_destroy(&ca); matrix_destroy(&cb); matrix_destroy(&cr); matrix_destroy(&er);
}

static void test_fixed_size() {
    // 4x4 fixed-size multiply
    using Mat4 = Eigen::Matrix<double, 4, 4>;
    Mat4 A, B, R;
    unsigned int s = 1;
    for (int r = 0; r < 4; ++r) for (int c = 0; c < 4; ++c) {
        s = s * 1664525u + 1013904223u;
        A(r,c) = static_cast<double>(static_cast<int>(s >> 8) % 10000) * 0.0001;
    }
    s = 2;
    for (int r = 0; r < 4; ++r) for (int c = 0; c < 4; ++c) {
        s = s * 1664525u + 1013904223u;
        B(r,c) = static_cast<double>(static_cast<int>(s >> 8) % 10000) * 0.0001;
    }
    R.noalias() = A * B;

    // Compare against dynamic
    Eigen::MatrixXd Ad(4,4), Bd(4,4), Rd(4,4);
    for (int r = 0; r < 4; ++r) for (int c = 0; c < 4; ++c) { Ad(r,c)=A(r,c); Bd(r,c)=B(r,c); }
    Rd.noalias() = Ad * Bd;

    bool ok = true;
    for (int r = 0; r < 4; ++r) for (int c = 0; c < 4; ++c)
        if (std::fabs(R(r,c) - Rd(r,c)) > TOL) { ok = false; break; }
    check("fixed_4x4_mul_matches_dynamic", ok);
}

static void test_expr_add3() {
    int N = 32;
    Eigen::MatrixXd A(N,N), B(N,N), C(N,N);
    fill_rand(A,1); fill_rand(B,2); fill_rand(C,3);

    // Eigen expression template: A + B + C
    Eigen::MatrixXd E = A + B + C;

    // Manual: two separate additions
    Eigen::MatrixXd M = A + B;
    M += C;

    bool ok = (E - M).cwiseAbs().maxCoeff() < TOL;
    check("expr_add3_matches_manual", ok);
}

int main() {
    test_transpose_vs_c(4);
    test_transpose_vs_c(16);
    test_mul_vs_c(4);
    test_mul_vs_c(16);
    test_add_vs_c(4);
    test_add_vs_c(16);
    test_fixed_size();
    test_expr_add3();

    std::printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail != 0 ? 1 : 0;
}
