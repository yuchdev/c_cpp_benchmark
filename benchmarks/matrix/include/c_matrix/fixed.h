#ifndef C_MATRIX_FIXED_H
#define C_MATRIX_FIXED_H

/*
 * Compile-time-sized matrix kernels for N = 2..16 (C11 only).
 *
 * C has no templates, so fixed_impl.inc is textually instantiated once per N,
 * producing FixedMatN / FixedVecN types and fixedN_<op>() static-inline kernels
 * (fixed2_add, fixed3_mul, ... fixed16_mul_add).  This is the C counterpart of
 * Eigen::Matrix<double, N, N>: the dimension is a literal in every loop bound.
 */

#define MATRIX_FIXED_MIN_N 2
#define MATRIX_FIXED_MAX_N 16

/* X-macro over every supported size: MATRIX_FIXED_FOREACH_SIZE(X) expands to X(2) ... X(16). */
#define MATRIX_FIXED_FOREACH_SIZE(X)                                                            \
    X(2) X(3) X(4) X(5) X(6) X(7) X(8) X(9) X(10) X(11) X(12) X(13) X(14) X(15) X(16)

#define MATRIX_FIXED_N 2
#include "c_matrix/fixed_impl.inc"
#undef MATRIX_FIXED_N
#define MATRIX_FIXED_N 3
#include "c_matrix/fixed_impl.inc"
#undef MATRIX_FIXED_N
#define MATRIX_FIXED_N 4
#include "c_matrix/fixed_impl.inc"
#undef MATRIX_FIXED_N
#define MATRIX_FIXED_N 5
#include "c_matrix/fixed_impl.inc"
#undef MATRIX_FIXED_N
#define MATRIX_FIXED_N 6
#include "c_matrix/fixed_impl.inc"
#undef MATRIX_FIXED_N
#define MATRIX_FIXED_N 7
#include "c_matrix/fixed_impl.inc"
#undef MATRIX_FIXED_N
#define MATRIX_FIXED_N 8
#include "c_matrix/fixed_impl.inc"
#undef MATRIX_FIXED_N
#define MATRIX_FIXED_N 9
#include "c_matrix/fixed_impl.inc"
#undef MATRIX_FIXED_N
#define MATRIX_FIXED_N 10
#include "c_matrix/fixed_impl.inc"
#undef MATRIX_FIXED_N
#define MATRIX_FIXED_N 11
#include "c_matrix/fixed_impl.inc"
#undef MATRIX_FIXED_N
#define MATRIX_FIXED_N 12
#include "c_matrix/fixed_impl.inc"
#undef MATRIX_FIXED_N
#define MATRIX_FIXED_N 13
#include "c_matrix/fixed_impl.inc"
#undef MATRIX_FIXED_N
#define MATRIX_FIXED_N 14
#include "c_matrix/fixed_impl.inc"
#undef MATRIX_FIXED_N
#define MATRIX_FIXED_N 15
#include "c_matrix/fixed_impl.inc"
#undef MATRIX_FIXED_N
#define MATRIX_FIXED_N 16
#include "c_matrix/fixed_impl.inc"
#undef MATRIX_FIXED_N

#endif /* C_MATRIX_FIXED_H */
