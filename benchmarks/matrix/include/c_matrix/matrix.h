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

/* ---- Scenario kernels (see src/c/scenarios.c) ----
 * Hand-written C counterparts of the Eigen kernels in cpp_matrix/eigen_scenarios.hpp.
 * Each one does the same logical work as its Eigen twin on the same row-major data. */

/* out = terms[0] + terms[1] + ... + terms[k-1]  (k >= 2), composed from k-1 binary
 * matrix_add() passes that ping-pong through tmp_a / tmp_b (same shape as out). */
void matrix_add_chain(const Matrix *const *terms, size_t k,
                      Matrix *tmp_a, Matrix *tmp_b, Matrix *out);

/* Copy the bs x bs block at (sr,sc) of src into the block at (dr,dc) of dst. */
void matrix_block_copy(const Matrix *src, size_t sr, size_t sc,
                       Matrix *dst, size_t dr, size_t dc, size_t bs);

/* out[orow.., ocol..] = a[ar.., ac..] * b[br.., bc..], all three bs x bs blocks living
 * inside larger matrices (leading dimension = each matrix's own column count). */
void matrix_block_mul(const Matrix *a, size_t ar, size_t ac,
                      const Matrix *b, size_t br, size_t bc,
                      Matrix *out, size_t orow, size_t ocol, size_t bs);

/* Solve L * X = B for X; only the lower triangle (incl. diagonal) of L is read.
 * L is n x n, B and X are n x m. */
void matrix_trsm_lower(const Matrix *l, const Matrix *b, Matrix *x);

/* s = lower triangle of A * A^T (A is n x k, s is n x n). The strict upper triangle
 * of s is not touched. */
void matrix_syrk_lower(const Matrix *a, Matrix *s);

/* "Valid" 3x3 convolution: out is (in->rows-2) x (in->cols-2); kernel is row-major. */
void matrix_conv3x3(const Matrix *in, const double kernel[9], Matrix *out);

/* out[i] = T * pts[i] for every 4-component point (row i of pts); T is 4x4, pts and
 * out are M x 4 (an array of M homogeneous points laid out contiguously). */
void matrix_transform_points4(const Matrix *t, const Matrix *pts, Matrix *out);

/* Deterministic synthetic test image in [0,1]: checkerboard + disc + gradient + noise. */
void matrix_fill_image(Matrix *m, unsigned int seed);

/* Lower-triangular, well-conditioned system matrix derived from src (square):
 * strict lower = src/n, diagonal = 1 + src, strict upper = 0. */
void matrix_make_lower_triangular(const Matrix *src, Matrix *l);

/* Write m (values clamped to [0,1]) as an 8-bit binary PGM. Returns 0 on success. */
int matrix_write_pgm(const Matrix *m, const char *path);

/* Printing (debug) */
void matrix_print(const Matrix *m, const char *label);

#ifdef __cplusplus
}
#endif

#endif /* C_MATRIX_H */
