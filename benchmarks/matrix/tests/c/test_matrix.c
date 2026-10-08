/*
 * C matrix correctness tests.
 * Exit 0 = all pass, nonzero = failure.
 */
#include "c_matrix/matrix.h"
#include "c_matrix/fixed.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

static int g_pass = 0, g_fail = 0;

static void check(const char *name, int cond) {
    if (cond) { printf("[PASS] %s\n", name); ++g_pass; }
    else       { printf("[FAIL] %s\n", name); ++g_fail; }
}

static int mat_approx_eq(const Matrix *a, const Matrix *b, double tol) {
    if (a->rows != b->rows || a->cols != b->cols) return 0;
    size_t n = a->rows * a->cols;
    for (size_t i = 0; i < n; ++i)
        if (fabs(a->data[i] - b->data[i]) > tol) return 0;
    return 1;
}

static void test_transpose(void) {
    /* 2x3 -> 3x2 */
    Matrix a = matrix_create(2, 3);
    matrix_set(&a, 0,0, 1); matrix_set(&a, 0,1, 2); matrix_set(&a, 0,2, 3);
    matrix_set(&a, 1,0, 4); matrix_set(&a, 1,1, 5); matrix_set(&a, 1,2, 6);

    Matrix t = matrix_create(3, 2);
    matrix_transpose(&a, &t);

    check("transpose_shape", t.rows == 3 && t.cols == 2);
    check("transpose_val_0_0", matrix_get(&t, 0,0) == 1.0);
    check("transpose_val_1_0", matrix_get(&t, 1,0) == 2.0);
    check("transpose_val_2_1", matrix_get(&t, 2,1) == 6.0);

    matrix_destroy(&a);
    matrix_destroy(&t);
}

static void test_add_sub(void) {
    Matrix a = matrix_create(2, 2);
    Matrix b = matrix_create(2, 2);
    Matrix r = matrix_create(2, 2);

    matrix_set(&a,0,0,1); matrix_set(&a,0,1,2);
    matrix_set(&a,1,0,3); matrix_set(&a,1,1,4);
    matrix_set(&b,0,0,5); matrix_set(&b,0,1,6);
    matrix_set(&b,1,0,7); matrix_set(&b,1,1,8);

    matrix_add(&a, &b, &r);
    check("add_0_0", matrix_get(&r,0,0) == 6.0);
    check("add_1_1", matrix_get(&r,1,1) == 12.0);

    matrix_sub(&a, &b, &r);
    check("sub_0_0", matrix_get(&r,0,0) == -4.0);
    check("sub_1_1", matrix_get(&r,1,1) == -4.0);

    matrix_destroy(&a); matrix_destroy(&b); matrix_destroy(&r);
}

static void test_scale(void) {
    Matrix a = matrix_create(2, 2);
    Matrix r = matrix_create(2, 2);
    matrix_set(&a,0,0,2.0); matrix_set(&a,0,1,4.0);
    matrix_set(&a,1,0,6.0); matrix_set(&a,1,1,8.0);
    matrix_scale(&a, 0.5, &r);
    check("scale_0_0", matrix_get(&r,0,0) == 1.0);
    check("scale_1_1", matrix_get(&r,1,1) == 4.0);
    matrix_destroy(&a); matrix_destroy(&r);
}

static void test_mul(void) {
    /* 2x2 identity * A = A */
    Matrix I = matrix_create(2, 2);
    Matrix a = matrix_create(2, 2);
    Matrix r = matrix_create(2, 2);
    matrix_set(&I,0,0,1); matrix_set(&I,1,1,1);
    matrix_set(&a,0,0,3); matrix_set(&a,0,1,7);
    matrix_set(&a,1,0,2); matrix_set(&a,1,1,5);
    matrix_mul(&I, &a, &r);
    check("mul_identity_0_0", matrix_get(&r,0,0) == 3.0);
    check("mul_identity_0_1", matrix_get(&r,0,1) == 7.0);
    check("mul_identity_1_0", matrix_get(&r,1,0) == 2.0);
    check("mul_identity_1_1", matrix_get(&r,1,1) == 5.0);
    matrix_destroy(&I); matrix_destroy(&a); matrix_destroy(&r);
}

static void test_matvec(void) {
    Matrix a = matrix_create(2, 3);
    double x[3] = {1.0, 2.0, 3.0};
    double y[2] = {0.0, 0.0};
    /* row 0: [1 2 3]  dot [1 2 3] = 14 */
    /* row 1: [4 5 6]  dot [1 2 3] = 32 */
    matrix_set(&a,0,0,1); matrix_set(&a,0,1,2); matrix_set(&a,0,2,3);
    matrix_set(&a,1,0,4); matrix_set(&a,1,1,5); matrix_set(&a,1,2,6);
    matrix_matvec(&a, x, y);
    check("matvec_y0", fabs(y[0] - 14.0) < 1e-9);
    check("matvec_y1", fabs(y[1] - 32.0) < 1e-9);
    matrix_destroy(&a);
}

static void test_transpose_mul(void) {
    /* A^T * A for 3x2 A: result is 2x2 positive semi-definite */
    Matrix a = matrix_create(3, 2);
    Matrix out = matrix_create(2, 2);
    matrix_set(&a,0,0,1); matrix_set(&a,0,1,2);
    matrix_set(&a,1,0,3); matrix_set(&a,1,1,4);
    matrix_set(&a,2,0,5); matrix_set(&a,2,1,6);
    /* A^T * A = [[1+9+25, 2+12+30],[2+12+30, 4+16+36]] = [[35,44],[44,56]] */
    matrix_transpose_mul(&a, &a, &out);
    check("transpose_mul_0_0", fabs(matrix_get(&out,0,0) - 35.0) < 1e-9);
    check("transpose_mul_0_1", fabs(matrix_get(&out,0,1) - 44.0) < 1e-9);
    check("transpose_mul_1_1", fabs(matrix_get(&out,1,1) - 56.0) < 1e-9);
    matrix_destroy(&a); matrix_destroy(&out);
}

static void test_add3(void) {
    Matrix a = matrix_create(2, 2);
    Matrix b = matrix_create(2, 2);
    Matrix c = matrix_create(2, 2);
    Matrix r = matrix_create(2, 2);
    for (size_t i = 0; i < 4; ++i) { a.data[i] = 1; b.data[i] = 2; c.data[i] = 3; }
    matrix_add3(&a, &b, &c, &r);
    check("add3_all_6", r.data[0] == 6.0 && r.data[3] == 6.0);
    matrix_destroy(&a); matrix_destroy(&b); matrix_destroy(&c); matrix_destroy(&r);
}

static void test_mul_add(void) {
    /* I*A + A = 2*A */
    Matrix I = matrix_create(2, 2);
    Matrix a = matrix_create(2, 2);
    Matrix r = matrix_create(2, 2);
    matrix_set(&I,0,0,1); matrix_set(&I,1,1,1);
    matrix_set(&a,0,0,3); matrix_set(&a,0,1,7);
    matrix_set(&a,1,0,2); matrix_set(&a,1,1,5);
    matrix_mul_add(&I, &a, &a, &r);
    check("mul_add_0_0", fabs(matrix_get(&r,0,0) - 6.0) < 1e-9);
    check("mul_add_0_1", fabs(matrix_get(&r,0,1) - 14.0) < 1e-9);
    matrix_destroy(&I); matrix_destroy(&a); matrix_destroy(&r);
}


/* ------------------------------------------------------------------------------------
 * Scenario kernels (chain / block / tri / conv / batch / image / fixed-size)
 * ------------------------------------------------------------------------------------ */

static Matrix rand_matrix(size_t r, size_t c, unsigned int seed) {
    Matrix m = matrix_create(r, c);
    matrix_fill_rand(&m, seed);
    return m;
}

static void test_add_chain(void) {
    const size_t n = 9;
    int ok = 1;
    for (size_t k = 2; k <= 16; ++k) {
        Matrix terms[16];
        const Matrix *ptrs[16];
        Matrix expect = matrix_create(n, n);
        for (size_t i = 0; i < k; ++i) {
            terms[i] = rand_matrix(n, n, (unsigned int)(10 + i));
            ptrs[i] = &terms[i];
        }
        /* left-associated reference: ((t0 + t1) + t2) + ... */
        for (size_t e = 0; e < n * n; ++e) {
            double s = terms[0].data[e];
            for (size_t i = 1; i < k; ++i) s += terms[i].data[e];
            expect.data[e] = s;
        }
        Matrix ta = matrix_create(n, n), tb = matrix_create(n, n), out = matrix_create(n, n);
        matrix_add_chain(ptrs, k, &ta, &tb, &out);
        ok = ok && mat_approx_eq(&out, &expect, 0.0); /* same order => bit-identical */
        for (size_t i = 0; i < k; ++i) matrix_destroy(&terms[i]);
        matrix_destroy(&expect); matrix_destroy(&ta); matrix_destroy(&tb); matrix_destroy(&out);
    }
    check("add_chain_k2_to_k16_exact", ok);
}

static void test_add_chain_does_not_clobber_inputs(void) {
    const size_t n = 4;
    Matrix a = rand_matrix(n, n, 1), b = rand_matrix(n, n, 2), c = rand_matrix(n, n, 3);
    Matrix a0 = rand_matrix(n, n, 1), b0 = rand_matrix(n, n, 2), c0 = rand_matrix(n, n, 3);
    const Matrix *ptrs[3] = {&a, &b, &c};
    Matrix ta = matrix_create(n, n), tb = matrix_create(n, n), out = matrix_create(n, n);
    matrix_add_chain(ptrs, 3, &ta, &tb, &out);
    check("add_chain_inputs_untouched",
          mat_approx_eq(&a, &a0, 0.0) && mat_approx_eq(&b, &b0, 0.0) && mat_approx_eq(&c, &c0, 0.0));
    matrix_destroy(&a); matrix_destroy(&b); matrix_destroy(&c);
    matrix_destroy(&a0); matrix_destroy(&b0); matrix_destroy(&c0);
    matrix_destroy(&ta); matrix_destroy(&tb); matrix_destroy(&out);
}

static Matrix extract_block(const Matrix *m, size_t r0, size_t c0, size_t bs) {
    Matrix b = matrix_create(bs, bs);
    for (size_t i = 0; i < bs; ++i)
        for (size_t j = 0; j < bs; ++j)
            matrix_set(&b, i, j, matrix_get(m, r0 + i, c0 + j));
    return b;
}

static void test_block_copy(void) {
    const size_t n = 40, bs = 13;
    Matrix src = rand_matrix(n, n, 5), dst = rand_matrix(n, n, 6), before = rand_matrix(n, n, 6);
    matrix_block_copy(&src, 3, 5, &dst, 9, 1, bs);
    int ok = 1;
    for (size_t r = 0; r < n; ++r)
        for (size_t c = 0; c < n; ++c) {
            int inside = r >= 9 && r < 9 + bs && c >= 1 && c < 1 + bs;
            double want = inside ? matrix_get(&src, 3 + (r - 9), 5 + (c - 1)) : matrix_get(&before, r, c);
            if (matrix_get(&dst, r, c) != want) ok = 0;
        }
    check("block_copy_moves_block_and_nothing_else", ok);
    matrix_destroy(&src); matrix_destroy(&dst); matrix_destroy(&before);
}

static void test_block_mul(void) {
    const size_t n = 40, bs = 11;
    Matrix a = rand_matrix(n, n, 7), b = rand_matrix(n, n, 8);
    Matrix d = matrix_create(n, n), d0 = matrix_create(n, n);
    matrix_block_mul(&a, 3, 5, &b, 11, 7, &d, 1, 9, bs);

    Matrix ab = extract_block(&a, 3, 5, bs), bb = extract_block(&b, 11, 7, bs);
    Matrix ref = matrix_create(bs, bs);
    matrix_mul(&ab, &bb, &ref);
    int ok = 1;
    for (size_t r = 0; r < n; ++r)
        for (size_t c = 0; c < n; ++c) {
            int inside = r >= 1 && r < 1 + bs && c >= 9 && c < 9 + bs;
            double want = inside ? matrix_get(&ref, r - 1, c - 9) : matrix_get(&d0, r, c);
            if (fabs(matrix_get(&d, r, c) - want) > 1e-12) ok = 0;
        }
    check("block_mul_matches_extracted_mul_and_spares_outside", ok);
    matrix_destroy(&a); matrix_destroy(&b); matrix_destroy(&d); matrix_destroy(&d0);
    matrix_destroy(&ab); matrix_destroy(&bb); matrix_destroy(&ref);
}

static void test_trsm_lower(void) {
    const size_t sizes[] = {1, 2, 8, 33};
    int ok = 1, canary_ok = 1;
    for (int si = 0; si < 4; ++si) {
        size_t n = sizes[si];
        Matrix a = rand_matrix(n, n, 3), l = matrix_create(n, n);
        Matrix rhs = rand_matrix(n, n, 4), x = matrix_create(n, n), lx = matrix_create(n, n);
        matrix_make_lower_triangular(&a, &l);
        matrix_trsm_lower(&l, &rhs, &x);
        matrix_mul(&l, &x, &lx); /* l has an all-zero upper triangle, so this is exact L*X */
        ok = ok && mat_approx_eq(&lx, &rhs, 1e-9);

        /* Only the lower triangle may be read: poison the upper one with NaN. */
        Matrix lp = matrix_create(n, n), xp = matrix_create(n, n);
        memcpy(lp.data, l.data, n * n * sizeof(double));
        for (size_t i = 0; i < n; ++i)
            for (size_t j = i + 1; j < n; ++j) matrix_set(&lp, i, j, NAN);
        matrix_trsm_lower(&lp, &rhs, &xp);
        canary_ok = canary_ok && mat_approx_eq(&xp, &x, 0.0);

        matrix_destroy(&a); matrix_destroy(&l); matrix_destroy(&rhs); matrix_destroy(&x);
        matrix_destroy(&lx); matrix_destroy(&lp); matrix_destroy(&xp);
    }
    check("trsm_lower_solves_L_X_eq_B", ok);
    check("trsm_lower_ignores_upper_triangle", canary_ok);
}

static void test_make_lower_triangular(void) {
    const size_t n = 6;
    Matrix a = rand_matrix(n, n, 2), l = matrix_create(n, n);
    matrix_make_lower_triangular(&a, &l);
    int upper_zero = 1, diag_ok = 1, lower_ok = 1;
    for (size_t i = 0; i < n; ++i)
        for (size_t j = 0; j < n; ++j) {
            double v = matrix_get(&l, i, j);
            if (j > i && v != 0.0) upper_zero = 0;
            if (j == i && fabs(v - (1.0 + matrix_get(&a, i, i))) > 1e-15) diag_ok = 0;
            if (j < i && fabs(v - matrix_get(&a, i, j) / (double)n) > 1e-15) lower_ok = 0;
        }
    check("make_lower_triangular_upper_zero", upper_zero);
    check("make_lower_triangular_diag", diag_ok);
    check("make_lower_triangular_lower_scaled", lower_ok);
    matrix_destroy(&a); matrix_destroy(&l);
}

static void test_syrk_lower(void) {
    const size_t n = 17, k = 17;
    Matrix a = rand_matrix(n, k, 9), at = matrix_create(k, n), full = matrix_create(n, n);
    Matrix s = matrix_create(n, n);
    matrix_transpose(&a, &at);
    matrix_mul(&a, &at, &full);
    matrix_syrk_lower(&a, &s);
    int lower_ok = 1, upper_untouched = 1;
    for (size_t i = 0; i < n; ++i)
        for (size_t j = 0; j < n; ++j) {
            if (j <= i && fabs(matrix_get(&s, i, j) - matrix_get(&full, i, j)) > 1e-9) lower_ok = 0;
            if (j > i && matrix_get(&s, i, j) != 0.0) upper_untouched = 0;
        }
    check("syrk_lower_matches_A_At_lower_triangle", lower_ok);
    check("syrk_lower_leaves_upper_triangle_alone", upper_untouched);
    matrix_destroy(&a); matrix_destroy(&at); matrix_destroy(&full); matrix_destroy(&s);
}

static void test_conv3x3(void) {
    const size_t h = 7, w = 9;
    Matrix in = rand_matrix(h, w, 4), out = matrix_create(h - 2, w - 2);

    const double ident[9] = {0, 0, 0, 0, 1, 0, 0, 0, 0};
    matrix_conv3x3(&in, ident, &out);
    int ident_ok = 1;
    for (size_t r = 0; r < h - 2; ++r)
        for (size_t c = 0; c < w - 2; ++c)
            if (matrix_get(&out, r, c) != matrix_get(&in, r + 1, c + 1)) ident_ok = 0;
    check("conv3x3_identity_kernel_returns_center", ident_ok);

    const double shift[9] = {0, 0, 0, 0, 0, 1, 0, 0, 0}; /* picks the right-hand neighbour */
    matrix_conv3x3(&in, shift, &out);
    check("conv3x3_shift_kernel_picks_neighbour",
          matrix_get(&out, 2, 3) == matrix_get(&in, 3, 5));

    Matrix flat = matrix_create(h, w);
    for (size_t i = 0; i < h * w; ++i) flat.data[i] = 3.0;
    const double gauss[9] = {1 / 16.0, 2 / 16.0, 1 / 16.0, 2 / 16.0, 4 / 16.0,
                             2 / 16.0, 1 / 16.0, 2 / 16.0, 1 / 16.0};
    matrix_conv3x3(&flat, gauss, &out);
    int flat_ok = 1;
    for (size_t i = 0; i < out.rows * out.cols; ++i)
        if (fabs(out.data[i] - 3.0) > 1e-12) flat_ok = 0;
    check("conv3x3_gaussian_preserves_constant_image", flat_ok);
    matrix_destroy(&in); matrix_destroy(&out); matrix_destroy(&flat);
}

static void test_transform_points4(void) {
    const size_t m = 25;
    Matrix pts = rand_matrix(m, 4, 6), out = matrix_create(m, 4);
    Matrix id = matrix_create(4, 4);
    for (size_t i = 0; i < 4; ++i) matrix_set(&id, i, i, 1.0);
    matrix_transform_points4(&id, &pts, &out);
    check("transform_points4_identity", mat_approx_eq(&out, &pts, 0.0));

    Matrix t = rand_matrix(4, 4, 11), tref = matrix_create(m, 4), ptt = matrix_create(m, 4);
    Matrix tt = matrix_create(4, 4);
    matrix_transpose(&t, &tt);
    matrix_mul(&pts, &tt, &tref); /* row-major points: out = pts * T^T */
    matrix_transform_points4(&t, &pts, &ptt);
    check("transform_points4_matches_pts_times_Tt", mat_approx_eq(&ptt, &tref, 1e-12));
    matrix_destroy(&pts); matrix_destroy(&out); matrix_destroy(&id); matrix_destroy(&t);
    matrix_destroy(&tref); matrix_destroy(&ptt); matrix_destroy(&tt);
}

static void test_fill_image(void) {
    Matrix a = matrix_create(40, 40), b = matrix_create(40, 40), c = matrix_create(40, 40);
    matrix_fill_image(&a, 1); matrix_fill_image(&b, 1); matrix_fill_image(&c, 2);
    int in_range = 1;
    for (size_t i = 0; i < 1600; ++i) if (a.data[i] < 0.0 || a.data[i] > 1.0) in_range = 0;
    check("fill_image_in_unit_range", in_range);
    check("fill_image_deterministic", mat_approx_eq(&a, &b, 0.0));
    check("fill_image_seed_changes_noise", !mat_approx_eq(&a, &c, 0.0));
    matrix_destroy(&a); matrix_destroy(&b); matrix_destroy(&c);
}

static void test_write_pgm(void) {
    Matrix m = matrix_create(3, 5);
    for (size_t i = 0; i < 15; ++i) m.data[i] = (double)i / 14.0;
    m.data[0] = -1.0; /* clamps to 0 */
    m.data[14] = 2.0; /* clamps to 255 */
    const char *path = "test_matrix_out.pgm";
    int rc = matrix_write_pgm(&m, path);
    check("write_pgm_returns_zero", rc == 0);
    FILE *f = fopen(path, "rb");
    char magic[3] = {0};
    int w = 0, h = 0, maxv = 0;
    int parsed = f ? fscanf(f, "%2s %d %d %d", magic, &w, &h, &maxv) : 0;
    check("write_pgm_header", parsed == 4 && strcmp(magic, "P5") == 0 && w == 5 && h == 3 && maxv == 255);
    unsigned char px[15] = {0};
    size_t got = 0;
    if (f) { (void)fgetc(f); got = fread(px, 1, 15, f); fclose(f); }
    check("write_pgm_pixels_clamped", got == 15 && px[0] == 0 && px[14] == 255);
    remove(path);
    check("write_pgm_bad_path_fails", matrix_write_pgm(&m, "/nonexistent-dir/x.pgm") != 0);
    matrix_destroy(&m);
}

/* Compile-time-N kernels must reproduce the runtime-sized kernels for every N = 2..16.
 * FixedMatN is a contiguous double[N][N], so one raw comparison against the dynamic
 * (row-major) Matrix covers every size. */
static int fixed_equals(const double *fixed_data, const Matrix *dyn) {
    for (size_t i = 0; i < dyn->rows * dyn->cols; ++i)
        if (fabs(fixed_data[i] - dyn->data[i]) > 1e-12) return 0;
    return 1;
}

#define DEFINE_FIXED_CHECK(N)                                                                  \
    static int check_fixed_##N(void) {                                                         \
        FixedMat##N fa, fb, fc, fo;                                                            \
        FixedVec##N fx, fy;                                                                    \
        fixed##N##_fill_rand(&fa, 1);                                                          \
        fixed##N##_fill_rand(&fb, 2);                                                          \
        fixed##N##_fill_rand(&fc, 3);                                                          \
        Matrix a = rand_matrix(N, N, 1), b = rand_matrix(N, N, 2), c = rand_matrix(N, N, 3);  \
        Matrix o = matrix_create(N, N);                                                        \
        double x[N], y[N];                                                                     \
        for (int i = 0; i < N; ++i) { x[i] = i * 0.001; fx.v[i] = i * 0.001; }                 \
        int ok = fixed_equals(&fa.m[0][0], &a); /* LCG data identical to matrix_fill_rand */   \
        fixed##N##_transpose(&fa, &fo);          matrix_transpose(&a, &o);                     \
        ok = ok && fixed_equals(&fo.m[0][0], &o);                                              \
        fixed##N##_add(&fa, &fb, &fo);           matrix_add(&a, &b, &o);                       \
        ok = ok && fixed_equals(&fo.m[0][0], &o);                                              \
        fixed##N##_sub(&fa, &fb, &fo);           matrix_sub(&a, &b, &o);                       \
        ok = ok && fixed_equals(&fo.m[0][0], &o);                                              \
        fixed##N##_scale(&fa, 2.5, &fo);         matrix_scale(&a, 2.5, &o);                    \
        ok = ok && fixed_equals(&fo.m[0][0], &o);                                              \
        fixed##N##_mul(&fa, &fb, &fo);           matrix_mul(&a, &b, &o);                       \
        ok = ok && fixed_equals(&fo.m[0][0], &o);                                              \
        fixed##N##_transpose_mul(&fa, &fb, &fo); matrix_transpose_mul(&a, &b, &o);             \
        ok = ok && fixed_equals(&fo.m[0][0], &o);                                              \
        fixed##N##_add3(&fa, &fb, &fc, &fo);     matrix_add3(&a, &b, &c, &o);                  \
        ok = ok && fixed_equals(&fo.m[0][0], &o);                                              \
        fixed##N##_mul_add(&fa, &fb, &fc, &fo);  matrix_mul_add(&a, &b, &c, &o);               \
        ok = ok && fixed_equals(&fo.m[0][0], &o);                                              \
        fixed##N##_matvec(&fa, &fx, &fy);        matrix_matvec(&a, x, y);                      \
        for (int i = 0; i < N; ++i)                                                            \
            if (fabs(fy.v[i] - y[i]) > 1e-12) ok = 0;                                          \
        matrix_destroy(&a); matrix_destroy(&b); matrix_destroy(&c); matrix_destroy(&o);        \
        return ok;                                                                             \
    }

MATRIX_FIXED_FOREACH_SIZE(DEFINE_FIXED_CHECK)

static void test_fixed_kernels(void) {
    int all_ok = 1, failed_n = 0;
#define RUN_FIXED_CHECK(N)                                                                     \
    if (!check_fixed_##N()) { all_ok = 0; failed_n = N; }
    MATRIX_FIXED_FOREACH_SIZE(RUN_FIXED_CHECK)
#undef RUN_FIXED_CHECK
    if (!all_ok) printf("  (first mismatch at N=%d)\n", failed_n);
    check("fixed_kernels_match_dynamic_for_N_2_to_16", all_ok);
}

int main(void) {
    test_transpose();
    test_add_sub();
    test_scale();
    test_mul();
    test_matvec();
    test_transpose_mul();
    test_add3();
    test_mul_add();
    test_add_chain();
    test_add_chain_does_not_clobber_inputs();
    test_block_copy();
    test_block_mul();
    test_make_lower_triangular();
    test_trsm_lower();
    test_syrk_lower();
    test_conv3x3();
    test_transform_points4();
    test_fill_image();
    test_write_pgm();
    test_fixed_kernels();

    printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail != 0 ? 1 : 0;
}
