#include "c_matrix/matrix.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

Matrix matrix_create(size_t rows, size_t cols) {
    Matrix m;
    m.rows = rows;
    m.cols = cols;
    m.data = (double *)calloc(rows * cols, sizeof(double));
    assert(m.data != NULL);
    return m;
}

void matrix_destroy(Matrix *m) {
    free(m->data);
    m->data = NULL;
    m->rows = m->cols = 0;
}

void matrix_fill_zero(Matrix *m) {
    memset(m->data, 0, m->rows * m->cols * sizeof(double));
}

/* LCG same as eigen_fill_rand_fixed for cross-language reproducibility */
void matrix_fill_rand(Matrix *m, unsigned int seed) {
    unsigned int s = seed;
    size_t n = m->rows * m->cols;
    for (size_t i = 0; i < n; ++i) {
        s = s * 1664525u + 1013904223u;
        m->data[i] = (double)((int)(s >> 8) % 10000) * 0.0001;
    }
}

void matrix_print(const Matrix *m, const char *label) {
    printf("%s (%zux%zu):\n", label, m->rows, m->cols);
    for (size_t r = 0; r < m->rows && r < 8; ++r) {
        printf("  ");
        for (size_t c = 0; c < m->cols && c < 8; ++c)
            printf("%8.4f ", matrix_get(m, r, c));
        if (m->cols > 8) printf("...");
        printf("\n");
    }
    if (m->rows > 8) printf("  ...\n");
}
