/*
 * C++ / Eigen correctness tests.
 * Compares C and Eigen results within tolerance, and verifies Eigen-only results.
 */
#include "cpp_matrix/eigen_ops.hpp"
#include "cpp_matrix/eigen_scenarios.hpp"
#include "c_matrix/matrix.h"
#include <Eigen/Dense>
#include <array>
#include <cstdio>
#include <cmath>
#include <cstring>
#include <limits>
#include <utility>

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

// ---------------------------------------------------------------------------------------
// Scenario kernels: every Eigen kernel in cpp_matrix/eigen_scenarios.hpp (the code the
// benchmark driver actually times) must agree with its C twin on identical data.
// ---------------------------------------------------------------------------------------

static Matrix rand_c(size_t r, size_t c, unsigned int seed) {
    Matrix m = matrix_create(r, c);
    matrix_fill_rand(&m, seed);
    return m;
}

// Largest |difference| between an Eigen matrix and a C Matrix of the same shape.
static double max_diff(const Eigen::MatrixXd &e, const Matrix &m) {
    if (static_cast<size_t>(e.rows()) != m.rows || static_cast<size_t>(e.cols()) != m.cols)
        return std::numeric_limits<double>::infinity();
    double worst = 0.0;
    for (int r = 0; r < e.rows(); ++r)
        for (int c = 0; c < e.cols(); ++c) {
            double d = std::fabs(e(r, c) - matrix_get(&m, r, c));
            if (!(d <= worst)) worst = d; // also propagates NaN
        }
    return worst;
}

static void test_from_to_c_roundtrip() {
    Matrix m = rand_c(5, 7, 3);
    Eigen::MatrixXd e = scn::from_c(m);
    Matrix back = matrix_create(5, 7);
    scn::to_c(e, back);
    check("from_c_matches_element_access", max_diff(e, m) == 0.0);
    check("to_c_roundtrip", std::memcmp(m.data, back.data, 35 * sizeof(double)) == 0);
    matrix_destroy(&m); matrix_destroy(&back);
}

template <size_t K>
static bool chain_matches_c() {
    const size_t n = 12;
    std::array<Eigen::MatrixXd, 16> terms;
    std::array<Matrix, 16> cterms;
    const Matrix *ptrs[16];
    for (size_t i = 0; i < 16; ++i) {
        cterms[i] = rand_c(n, n, 20 + static_cast<unsigned int>(i));
        terms[i] = scn::from_c(cterms[i]);
        ptrs[i] = &cterms[i];
    }
    Eigen::MatrixXd out = Eigen::MatrixXd::Zero(n, n);
    scn::chain_sum<K>(out, terms);

    Matrix ta = matrix_create(n, n), tb = matrix_create(n, n), cout_ = matrix_create(n, n);
    matrix_add_chain(ptrs, K, &ta, &tb, &cout_);
    const bool ok = max_diff(out, cout_) < 1e-12;
    for (auto &c : cterms) matrix_destroy(&c);
    matrix_destroy(&ta); matrix_destroy(&tb); matrix_destroy(&cout_);
    return ok;
}

template <size_t... Ks>
static bool chain_all(std::index_sequence<Ks...>) {
    return (chain_matches_c<Ks + 2>() && ...);
}

static void test_chain() {
    // K = 2 .. 16: one fused Eigen expression vs K-1 binary C passes.
    check("chain_eigen_fused_matches_c_k2_to_k16", chain_all(std::make_index_sequence<15>{}));
}

static void test_block() {
    const size_t n = 48, bs = 17;
    Matrix ca = rand_c(n, n, 1), cb = rand_c(n, n, 2);
    Eigen::MatrixXd a = scn::from_c(ca), b = scn::from_c(cb);

    Eigen::MatrixXd d = Eigen::MatrixXd::Zero(n, n);
    Matrix cd = matrix_create(n, n);
    scn::block_copy(a, 3, 5, d, 9, 1, bs);
    matrix_block_copy(&ca, 3, 5, &cd, 9, 1, bs);
    check("block_copy_matches_c", max_diff(d, cd) == 0.0);

    Eigen::MatrixXd d2 = Eigen::MatrixXd::Zero(n, n);
    Matrix cd2 = matrix_create(n, n);
    scn::block_mul(a, 3, 5, b, 11, 7, d2, 1, 9, bs);
    matrix_block_mul(&ca, 3, 5, &cb, 11, 7, &cd2, 1, 9, bs);
    check("block_mul_matches_c", max_diff(d2, cd2) < 1e-9);
    matrix_destroy(&ca); matrix_destroy(&cb); matrix_destroy(&cd); matrix_destroy(&cd2);
}

static void test_tri() {
    for (size_t n : {1, 5, 32, 70}) {
        Matrix ca = rand_c(n, n, 3), cl = matrix_create(n, n), cr = rand_c(n, n, 4);
        matrix_make_lower_triangular(&ca, &cl);
        Eigen::MatrixXd a = scn::from_c(ca), l = scn::from_c(cl), rhs = scn::from_c(cr);

        Eigen::MatrixXd x(n, n);
        Matrix cx = matrix_create(n, n);
        scn::trsm_lower(l, rhs, x);
        matrix_trsm_lower(&cl, &cr, &cx);
        char name[64];
        std::snprintf(name, sizeof(name), "trsm_matches_c_n%zu", n);
        check(name, max_diff(x, cx) < 1e-9);

        // Eigen must not read the upper triangle either: poison it with NaN.
        Eigen::MatrixXd lp = l;
        for (size_t i = 0; i < n; ++i)
            for (size_t j = i + 1; j < n; ++j) lp(i, j) = std::numeric_limits<double>::quiet_NaN();
        Eigen::MatrixXd xp(n, n);
        scn::trsm_lower(lp, rhs, xp);
        std::snprintf(name, sizeof(name), "trsm_ignores_upper_triangle_n%zu", n);
        check(name, (xp - x).cwiseAbs().maxCoeff() == 0.0);

        Eigen::MatrixXd s = Eigen::MatrixXd::Constant(n, n, 99.0); // stale data must be cleared
        Matrix cs = matrix_create(n, n);
        scn::syrk_lower(a, s);
        matrix_syrk_lower(&ca, &cs);
        std::snprintf(name, sizeof(name), "syrk_matches_c_n%zu", n);
        check(name, max_diff(s, cs) < 1e-9);

        matrix_destroy(&ca); matrix_destroy(&cl); matrix_destroy(&cr);
        matrix_destroy(&cx); matrix_destroy(&cs);
    }
}

static void test_conv() {
    const size_t n = 37;
    Matrix ci = matrix_create(n, n);
    matrix_fill_image(&ci, 1);
    Eigen::MatrixXd img = scn::from_c(ci);
    const double k[9] = {1 / 16.0, 2 / 16.0, 1 / 16.0, 2 / 16.0, 4 / 16.0,
                         2 / 16.0, 1 / 16.0, 2 / 16.0, 1 / 16.0};
    Eigen::MatrixXd out = Eigen::MatrixXd::Zero(n - 2, n - 2);
    Matrix co = matrix_create(n - 2, n - 2);
    scn::conv3x3(img, k, out);
    matrix_conv3x3(&ci, k, &co);
    check("conv3x3_matches_c", max_diff(out, co) < 1e-12);

    // An asymmetric kernel catches transposed block offsets that a symmetric one would hide.
    const double asym[9] = {0, 1, 0, 2, 0, 3, 0, 4, 0};
    scn::conv3x3(img, asym, out);
    matrix_conv3x3(&ci, asym, &co);
    check("conv3x3_asymmetric_kernel_matches_c", max_diff(out, co) < 1e-12);
    matrix_destroy(&ci); matrix_destroy(&co);
}

static void test_transform_points() {
    const size_t m = 101;
    Matrix ct = rand_c(4, 4, 5), cp = rand_c(m, 4, 6), cout_ = matrix_create(m, 4);
    Eigen::Matrix4d t;
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c) t(r, c) = matrix_get(&ct, r, c);
    Matrix eout = matrix_create(m, 4);
    scn::transform_points4(t, cp.data, eout.data, static_cast<Eigen::Index>(m));
    matrix_transform_points4(&ct, &cp, &cout_);
    bool ok = true;
    for (size_t i = 0; i < m * 4; ++i)
        if (std::fabs(eout.data[i] - cout_.data[i]) > 1e-12) ok = false;
    check("transform_points4_map_matches_c", ok);
    matrix_destroy(&ct); matrix_destroy(&cp); matrix_destroy(&cout_); matrix_destroy(&eout);
}

template <int N>
static bool fixed_matches_c() {
    using F = scn::Fixed<N>;
    using Mat = typename F::Mat;
    using Vec = typename F::Vec;
    Matrix ca = rand_c(N, N, 1), cb = rand_c(N, N, 2), cc = rand_c(N, N, 3);
    Matrix co = matrix_create(N, N);
    auto load = [](const Matrix &m) {
        Mat r;
        for (int i = 0; i < N; ++i)
            for (int j = 0; j < N; ++j) r(i, j) = matrix_get(&m, i, j);
        return r;
    };
    const Mat a = load(ca), b = load(cb), c = load(cc);
    Mat out = Mat::Zero();
    auto same = [&](const Mat &e, const Matrix &m) {
        for (int i = 0; i < N; ++i)
            for (int j = 0; j < N; ++j)
                if (std::fabs(e(i, j) - matrix_get(&m, i, j)) > 1e-12) return false;
        return true;
    };
    bool ok = true;
    F::transpose(a, out);        matrix_transpose(&ca, &co);              ok = ok && same(out, co);
    F::add(a, b, out);           matrix_add(&ca, &cb, &co);               ok = ok && same(out, co);
    F::sub(a, b, out);           matrix_sub(&ca, &cb, &co);               ok = ok && same(out, co);
    F::scale(a, 2.5, out);       matrix_scale(&ca, 2.5, &co);             ok = ok && same(out, co);
    F::mul(a, b, out);           matrix_mul(&ca, &cb, &co);               ok = ok && same(out, co);
    F::transpose_mul(a, b, out); matrix_transpose_mul(&ca, &cb, &co);     ok = ok && same(out, co);
    F::add3(a, b, c, out);       matrix_add3(&ca, &cb, &cc, &co);         ok = ok && same(out, co);
    F::mul_add(a, b, c, out);    matrix_mul_add(&ca, &cb, &cc, &co);      ok = ok && same(out, co);

    Vec x, y;
    double cx[N], cy[N];
    for (int i = 0; i < N; ++i) { x(i) = i * 0.001; cx[i] = i * 0.001; }
    F::matvec(a, x, y);
    matrix_matvec(&ca, cx, cy);
    for (int i = 0; i < N; ++i)
        if (std::fabs(y(i) - cy[i]) > 1e-12) ok = false;
    matrix_destroy(&ca); matrix_destroy(&cb); matrix_destroy(&cc); matrix_destroy(&co);
    return ok;
}

template <int... Ns>
static bool fixed_all(std::integer_sequence<int, Ns...>) {
    return (fixed_matches_c<Ns + 2>() && ...);
}

static void test_fixed_sweep() {
    check("fixed_eigen_matches_c_for_N_2_to_16", fixed_all(std::make_integer_sequence<int, 15>{}));
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
    test_from_to_c_roundtrip();
    test_chain();
    test_block();
    test_tri();
    test_conv();
    test_transform_points();
    test_fixed_sweep();

    std::printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail != 0 ? 1 : 0;
}
