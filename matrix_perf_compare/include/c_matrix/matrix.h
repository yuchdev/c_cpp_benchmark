#ifndef C_MATRIX_H
#define C_MATRIX_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Row-major double matrix */
typedef struct {
    size_t rows;
    size_t cols;
    double *data;
} Matrix;

/* Lifecycle */
Matrix matrix_create(size_t rows, size_t cols);
void   matrix_destroy(Matrix *m);

/* Element access */
static inline double matrix_get(const Matrix *m, size_t r, size_t c) {
    return m->data[r * m->cols + c];
}
static inline void matrix_set(Matrix *m, size_t r, size_t c, double v) {
    m->data[r * m->cols + c] = v;
}

/* Initialization */
void matrix_fill_zero(Matrix *m);
void matrix_fill_rand(Matrix *m, unsigned int seed);   /* deterministic pseudo-random */

/* Basic operations - out must be pre-allocated with correct size */
void matrix_transpose(const Matrix *a, Matrix *out);    /* out: a->cols x a->rows */
void matrix_add(const Matrix *a, const Matrix *b, Matrix *out);
void matrix_sub(const Matrix *a, const Matrix *b, Matrix *out);
void matrix_scale(const Matrix *a, double s, Matrix *out);
void matrix_mul(const Matrix *a, const Matrix *b, Matrix *out);  /* out: a->rows x b->cols */
void matrix_matvec(const Matrix *a, const double *x, double *y); /* y = A*x  (len a->cols / a->rows) */

/* Chained helpers */
void matrix_transpose_mul(const Matrix *a, const Matrix *b, Matrix *out); /* out = A^T * B */
void matrix_add3(const Matrix *a, const Matrix *b, const Matrix *c, Matrix *out); /* out = a+b+c */
void matrix_mul_add(const Matrix *a, const Matrix *b, const Matrix *c, Matrix *out); /* out = A*B + C */

/* Printing (debug) */
void matrix_print(const Matrix *m, const char *label);

#ifdef __cplusplus
}
#endif

#endif /* C_MATRIX_H */
