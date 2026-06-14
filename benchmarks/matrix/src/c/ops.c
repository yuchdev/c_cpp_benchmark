#include "c_matrix/matrix.h"
#include <assert.h>
#include <string.h>

void matrix_transpose(const Matrix *a, Matrix *out) {
    assert(out->rows == a->cols && out->cols == a->rows);
    for (size_t r = 0; r < a->rows; ++r)
        for (size_t c = 0; c < a->cols; ++c)
            matrix_set(out, c, r, matrix_get(a, r, c));
}

void matrix_add(const Matrix *a, const Matrix *b, Matrix *out) {
    assert(a->rows == b->rows && a->cols == b->cols);
    assert(out->rows == a->rows && out->cols == a->cols);
    size_t n = a->rows * a->cols;
    for (size_t i = 0; i < n; ++i)
        out->data[i] = a->data[i] + b->data[i];
}

void matrix_sub(const Matrix *a, const Matrix *b, Matrix *out) {
    assert(a->rows == b->rows && a->cols == b->cols);
    assert(out->rows == a->rows && out->cols == a->cols);
    size_t n = a->rows * a->cols;
    for (size_t i = 0; i < n; ++i)
        out->data[i] = a->data[i] - b->data[i];
}

void matrix_scale(const Matrix *a, double s, Matrix *out) {
    assert(out->rows == a->rows && out->cols == a->cols);
    size_t n = a->rows * a->cols;
    for (size_t i = 0; i < n; ++i)
        out->data[i] = a->data[i] * s;
}

void matrix_mul(const Matrix *a, const Matrix *b, Matrix *out) {
    assert(a->cols == b->rows);
    assert(out->rows == a->rows && out->cols == b->cols);
    size_t M = a->rows, K = a->cols, N = b->cols;
    memset(out->data, 0, M * N * sizeof(double));
    for (size_t i = 0; i < M; ++i)
        for (size_t k = 0; k < K; ++k) {
            double aik = matrix_get(a, i, k);
            for (size_t j = 0; j < N; ++j)
                out->data[i * N + j] += aik * matrix_get(b, k, j);
        }
}

void matrix_matvec(const Matrix *a, const double *x, double *y) {
    size_t M = a->rows, N = a->cols;
    for (size_t i = 0; i < M; ++i) {
        double s = 0.0;
        for (size_t j = 0; j < N; ++j)
            s += matrix_get(a, i, j) * x[j];
        y[i] = s;
    }
}

void matrix_transpose_mul(const Matrix *a, const Matrix *b, Matrix *out) {
    /* out = A^T * B  => shape: a->cols x b->cols */
    assert(a->rows == b->rows);
    assert(out->rows == a->cols && out->cols == b->cols);
    size_t K = a->rows, M = a->cols, N = b->cols;
    memset(out->data, 0, M * N * sizeof(double));
    for (size_t k = 0; k < K; ++k)
        for (size_t i = 0; i < M; ++i) {
            double ati = matrix_get(a, k, i);
            for (size_t j = 0; j < N; ++j)
                out->data[i * N + j] += ati * matrix_get(b, k, j);
        }
}

void matrix_add3(const Matrix *a, const Matrix *b, const Matrix *c, Matrix *out) {
    assert(a->rows == b->rows && b->rows == c->rows);
    assert(a->cols == b->cols && b->cols == c->cols);
    assert(out->rows == a->rows && out->cols == a->cols);
    size_t n = a->rows * a->cols;
    for (size_t i = 0; i < n; ++i)
        out->data[i] = a->data[i] + b->data[i] + c->data[i];
}

void matrix_mul_add(const Matrix *a, const Matrix *b, const Matrix *c, Matrix *out) {
    /* out = A*B + C */
    assert(a->cols == b->rows);
    assert(c->rows == a->rows && c->cols == b->cols);
    assert(out->rows == a->rows && out->cols == b->cols);
    matrix_mul(a, b, out);
    size_t n = out->rows * out->cols;
    for (size_t i = 0; i < n; ++i)
        out->data[i] += c->data[i];
}
