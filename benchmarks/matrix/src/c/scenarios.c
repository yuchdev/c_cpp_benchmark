/*
 * Hand-written C kernels for the matrix "scenario" benchmarks.
 *
 * Each function is the portable-C twin of an Eigen kernel in
 * cpp_matrix/eigen_scenarios.hpp: same logical work, same row-major data.
 * They deliberately use the plain idioms a C programmer would reach for
 * (binary add passes through temporaries, row-by-row memcpy, i-k-j loops,
 * row dot products) rather than hand-fused or hand-vectorised variants.
 */
#include "c_matrix/matrix.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

void matrix_add_chain(const Matrix *const *terms, size_t k,
                      Matrix *tmp_a, Matrix *tmp_b, Matrix *out) {
    assert(k >= 2);
    Matrix *bufs[2] = {tmp_a, tmp_b};
    const Matrix *acc = terms[0];
    for (size_t i = 1; i < k; ++i) {
        Matrix *dst = (i == k - 1) ? out : bufs[(i - 1) & 1u];
        matrix_add(acc, terms[i], dst);
        acc = dst;
    }
}

void matrix_block_copy(const Matrix *src, size_t sr, size_t sc,
                       Matrix *dst, size_t dr, size_t dc, size_t bs) {
    assert(sr + bs <= src->rows && sc + bs <= src->cols);
    assert(dr + bs <= dst->rows && dc + bs <= dst->cols);
    for (size_t i = 0; i < bs; ++i)
        memcpy(&dst->data[(dr + i) * dst->cols + dc],
               &src->data[(sr + i) * src->cols + sc], bs * sizeof(double));
}

void matrix_block_mul(const Matrix *a, size_t ar, size_t ac,
                      const Matrix *b, size_t br, size_t bc,
                      Matrix *out, size_t orow, size_t ocol, size_t bs) {
    assert(ar + bs <= a->rows && ac + bs <= a->cols);
    assert(br + bs <= b->rows && bc + bs <= b->cols);
    assert(orow + bs <= out->rows && ocol + bs <= out->cols);
    const size_t lda = a->cols, ldb = b->cols, ldo = out->cols;
    for (size_t i = 0; i < bs; ++i) {
        double *o = &out->data[(orow + i) * ldo + ocol];
        memset(o, 0, bs * sizeof(double));
        for (size_t k = 0; k < bs; ++k) {
            const double aik = a->data[(ar + i) * lda + ac + k];
            const double *brow = &b->data[(br + k) * ldb + bc];
            for (size_t j = 0; j < bs; ++j)
                o[j] += aik * brow[j];
        }
    }
}

void matrix_trsm_lower(const Matrix *l, const Matrix *b, Matrix *x) {
    assert(l->rows == l->cols);
    assert(b->rows == l->rows && x->rows == b->rows && x->cols == b->cols);
    const size_t n = l->rows, m = b->cols;
    memcpy(x->data, b->data, n * m * sizeof(double));
    for (size_t i = 0; i < n; ++i) {
        double *xi = &x->data[i * m];
        for (size_t k = 0; k < i; ++k) {
            const double lik = l->data[i * n + k];
            const double *xk = &x->data[k * m];
            for (size_t j = 0; j < m; ++j)
                xi[j] -= lik * xk[j];
        }
        const double inv = 1.0 / l->data[i * n + i];
        for (size_t j = 0; j < m; ++j)
            xi[j] *= inv;
    }
}

void matrix_syrk_lower(const Matrix *a, Matrix *s) {
    assert(s->rows == a->rows && s->cols == a->rows);
    const size_t n = a->rows, k = a->cols;
    for (size_t i = 0; i < n; ++i) {
        const double *ai = &a->data[i * k];
        for (size_t j = 0; j <= i; ++j) {
            const double *aj = &a->data[j * k];
            double sum = 0.0;
            for (size_t p = 0; p < k; ++p)
                sum += ai[p] * aj[p];
            s->data[i * n + j] = sum;
        }
    }
}

void matrix_conv3x3(const Matrix *in, const double kernel[9], Matrix *out) {
    assert(in->rows >= 3 && in->cols >= 3);
    assert(out->rows == in->rows - 2 && out->cols == in->cols - 2);
    const size_t oh = out->rows, ow = out->cols, w = in->cols;
    for (size_t r = 0; r < oh; ++r) {
        const double *p0 = &in->data[r * w];
        const double *p1 = p0 + w;
        const double *p2 = p1 + w;
        double *o = &out->data[r * ow];
        for (size_t c = 0; c < ow; ++c)
            o[c] = kernel[0] * p0[c] + kernel[1] * p0[c + 1] + kernel[2] * p0[c + 2] +
                   kernel[3] * p1[c] + kernel[4] * p1[c + 1] + kernel[5] * p1[c + 2] +
                   kernel[6] * p2[c] + kernel[7] * p2[c + 1] + kernel[8] * p2[c + 2];
    }
}

void matrix_transform_points4(const Matrix *t, const Matrix *pts, Matrix *out) {
    assert(t->rows == 4 && t->cols == 4);
    assert(pts->cols == 4 && out->rows == pts->rows && out->cols == 4);
    assert(pts->data != out->data);
    const double *T = t->data;
    const size_t m = pts->rows;
    for (size_t i = 0; i < m; ++i) {
        const double *p = &pts->data[4 * i];
        double *q = &out->data[4 * i];
        for (size_t r = 0; r < 4; ++r)
            q[r] = T[4 * r] * p[0] + T[4 * r + 1] * p[1] +
                   T[4 * r + 2] * p[2] + T[4 * r + 3] * p[3];
    }
}

void matrix_fill_image(Matrix *m, unsigned int seed) {
    unsigned int s = seed;
    const double cy = (double)m->rows / 2.0, cx = (double)m->cols / 2.0;
    const double rad = (double)m->rows / 4.0;
    const double span = (double)(m->rows + m->cols);
    for (size_t r = 0; r < m->rows; ++r)
        for (size_t c = 0; c < m->cols; ++c) {
            double v = (((r >> 4) + (c >> 4)) & 1u) ? 0.85 : 0.15;
            const double dy = (double)r - cy, dx = (double)c - cx;
            if (dx * dx + dy * dy < rad * rad)
                v = 1.0 - v;
            s = s * 1664525u + 1013904223u;
            const double noise = (double)((s >> 8) % 1000u) * 0.0001;
            v = v * 0.8 + 0.1 * (double)(r + c) / span + noise;
            m->data[r * m->cols + c] = v < 0.0 ? 0.0 : (v > 1.0 ? 1.0 : v);
        }
}

void matrix_make_lower_triangular(const Matrix *src, Matrix *l) {
    assert(src->rows == src->cols && l->rows == src->rows && l->cols == src->cols);
    const size_t n = src->rows;
    const double inv_n = 1.0 / (double)n;
    for (size_t i = 0; i < n; ++i)
        for (size_t j = 0; j < n; ++j) {
            const double v = src->data[i * n + j];
            l->data[i * n + j] = j < i ? v * inv_n : (j == i ? 1.0 + v : 0.0);
        }
}

int matrix_write_pgm(const Matrix *m, const char *path) {
    FILE *f = fopen(path, "wb");
    if (!f)
        return 1;
    fprintf(f, "P5\n%zu %zu\n255\n", m->cols, m->rows);
    const size_t n = m->rows * m->cols;
    for (size_t i = 0; i < n; ++i) {
        double v = m->data[i];
        v = v < 0.0 ? 0.0 : (v > 1.0 ? 1.0 : v);
        fputc((int)(v * 255.0 + 0.5), f);
    }
    return fclose(f) == 0 ? 0 : 1;
}
