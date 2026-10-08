#pragma once
// Eigen kernels for the matrix "scenario" benchmarks.
//
// Each function is the idiomatic Eigen twin of a C kernel in c_matrix/matrix.h
// (src/c/scenarios.c) or c_matrix/fixed.h.  The C++ benchmark driver and the
// C-vs-Eigen cross-check tests both call these, so the code that is timed is
// exactly the code that is verified.  Everything is header-only and inline so
// the compiler sees the whole expression at the call site.
#include "c_matrix/matrix.h"
#include <Eigen/Dense>
#include <array>
#include <cstddef>
#include <utility>

namespace scn {

using Eigen::Index;
using Eigen::MatrixXd;

// ---- conversions between the C library's row-major Matrix and Eigen ---------------------

inline MatrixXd from_c(const Matrix& m) {
    MatrixXd e(static_cast<Index>(m.rows), static_cast<Index>(m.cols));
    for (Index r = 0; r < e.rows(); ++r)
        for (Index c = 0; c < e.cols(); ++c)
            e(r, c) = m.data[static_cast<std::size_t>(r) * m.cols + static_cast<std::size_t>(c)];
    return e;
}

// `m` must already have the same shape as `e`.
inline void to_c(const MatrixXd& e, Matrix& m) {
    for (Index r = 0; r < e.rows(); ++r)
        for (Index c = 0; c < e.cols(); ++c)
            m.data[static_cast<std::size_t>(r) * m.cols + static_cast<std::size_t>(c)] = e(r, c);
}

// ---- chain: out = m[0] + m[1] + ... + m[K-1] as ONE fused expression --------------------
// The fold builds ((m0 + m1) + m2) + ... as a single expression template, so Eigen makes
// exactly one pass over memory regardless of K (the C version needs K-1 passes).

template <std::size_t... Is, std::size_t Cap>
inline void chain_sum_impl(MatrixXd& out, const std::array<MatrixXd, Cap>& m,
                           std::index_sequence<Is...>) {
    out.noalias() = (... + m[Is]);
}

template <std::size_t K, std::size_t Cap>
inline void chain_sum(MatrixXd& out, const std::array<MatrixXd, Cap>& m) {
    static_assert(K >= 2 && K <= Cap, "chain depth must be in [2, Cap]");
    chain_sum_impl(out, m, std::make_index_sequence<K>{});
}

// ---- block: submatrices of larger matrices, addressed in place via Block<> --------------

inline void block_copy(const MatrixXd& src, Index sr, Index sc, MatrixXd& dst, Index dr,
                       Index dc, Index bs) {
    dst.block(dr, dc, bs, bs) = src.block(sr, sc, bs, bs);
}

inline void block_mul(const MatrixXd& a, Index ar, Index ac, const MatrixXd& b, Index br,
                      Index bc, MatrixXd& out, Index orow, Index ocol, Index bs) {
    out.block(orow, ocol, bs, bs).noalias() = a.block(ar, ac, bs, bs) * b.block(br, bc, bs, bs);
}

// ---- tri: only the triangle that matters is touched --------------------------------------

// Solve L * X = B; only the lower triangle of L is read.
inline void trsm_lower(const MatrixXd& l, const MatrixXd& b, MatrixXd& x) {
    x = b;
    l.triangularView<Eigen::Lower>().solveInPlace(x);
}

// s = lower triangle of A * A^T (the strict upper triangle stays zero).
inline void syrk_lower(const MatrixXd& a, MatrixXd& s) {
    s.setZero();
    s.selfadjointView<Eigen::Lower>().rankUpdate(a);
}

// ---- conv: 3x3 "valid" convolution as nine shifted, scaled views fused into one pass -----

inline void conv3x3(const MatrixXd& in, const double k[9], MatrixXd& out) {
    const Index oh = in.rows() - 2, ow = in.cols() - 2;
    out.noalias() = k[0] * in.block(0, 0, oh, ow) + k[1] * in.block(0, 1, oh, ow) +
                    k[2] * in.block(0, 2, oh, ow) + k[3] * in.block(1, 0, oh, ow) +
                    k[4] * in.block(1, 1, oh, ow) + k[5] * in.block(1, 2, oh, ow) +
                    k[6] * in.block(2, 0, oh, ow) + k[7] * in.block(2, 1, oh, ow) +
                    k[8] * in.block(2, 2, oh, ow);
}

// ---- batch: one 4x4 transform over M points, working in place on caller-owned memory -----
// The C library stores points as an M x 4 row-major array, i.e. 4 contiguous doubles per
// point, which is exactly a column-major 4 x M Eigen matrix: Map<> reinterprets the same
// buffer with no copy.

using Points4 = Eigen::Matrix<double, 4, Eigen::Dynamic>;

inline void transform_points4(const Eigen::Matrix4d& t, const double* pts, double* out,
                              Index m) {
    Eigen::Map<const Points4> x(pts, 4, m);
    Eigen::Map<Points4> y(out, 4, m);
    y.noalias() = t * x;
}

// ---- fixed: compile-time-sized kernels (twins of c_matrix/fixed.h) -----------------------

template <int N>
struct Fixed {
    using Mat = Eigen::Matrix<double, N, N>;
    using Vec = Eigen::Matrix<double, N, 1>;

    static void transpose(const Mat& a, Mat& out) { out.noalias() = a.transpose(); }
    static void add(const Mat& a, const Mat& b, Mat& out) { out.noalias() = a + b; }
    static void sub(const Mat& a, const Mat& b, Mat& out) { out.noalias() = a - b; }
    static void scale(const Mat& a, double s, Mat& out) { out.noalias() = a * s; }
    static void matvec(const Mat& a, const Vec& x, Vec& y) { y.noalias() = a * x; }
    static void mul(const Mat& a, const Mat& b, Mat& out) { out.noalias() = a * b; }
    static void transpose_mul(const Mat& a, const Mat& b, Mat& out) {
        out.noalias() = a.transpose() * b;
    }
    static void add3(const Mat& a, const Mat& b, const Mat& c, Mat& out) {
        out.noalias() = a + b + c;
    }
    static void mul_add(const Mat& a, const Mat& b, const Mat& c, Mat& out) {
        out.noalias() = a * b + c;
    }
};

} // namespace scn
