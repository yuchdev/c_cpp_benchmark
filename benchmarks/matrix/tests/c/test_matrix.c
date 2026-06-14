/*
 * C matrix correctness tests.
 * Exit 0 = all pass, nonzero = failure.
 */
#include "c_matrix/matrix.h"
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

int main(void) {
    test_transpose();
    test_add_sub();
    test_scale();
    test_mul();
    test_matvec();
    test_transpose_mul();
    test_add3();
    test_mul_add();

    printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail != 0 ? 1 : 0;
}
