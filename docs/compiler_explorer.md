# Compiler Explorer Examples

[Compiler Explorer](https://godbolt.org) (godbolt.org) lets you inspect the assembly generated for C and C++ code side-by-side. The five examples below illustrate the key differences highlighted by this benchmark.

---

## Example 1 - Small Fixed-size Transpose (4×4): Eigen vs C

**Goal:** show that Eigen's fixed-size transpose emits a small, fully-unrolled sequence of moves, while the C version requires a loop with runtime bounds checks.

### C++ (Eigen fixed 4×4)

```cpp
// Compiler: x86-64 GCC 13, flags: -O2 -std=c++17
#include <Eigen/Dense>

using Mat4 = Eigen::Matrix<double, 4, 4>;

Mat4 transpose_fixed(const Mat4 &A) {
    return A.transpose();
}
```

Expected assembly: 16 `movsd` or `vmovsd` instructions (one per element), fully unrolled - no branch, no loop counter.

### C (runtime loop)

```c
// Compiler: x86-64 GCC 13, flags: -O2 -std=c11
#include "c_matrix/matrix.h"

void transpose_c(const Matrix *a, Matrix *out) {
    for (size_t r = 0; r < a->rows; ++r)
        for (size_t c = 0; c < a->cols; ++c)
            out->data[c * a->rows + r] = a->data[r * a->cols + c];
}
```

Expected assembly: two nested loops with `imul`/`add` address arithmetic. The compiler may auto-vectorise with `-O2 -march=native` but cannot unroll without knowing N.

---

## Example 2 - Fixed-size 4×4 Multiply: Eigen vs C

**Goal:** compare assembly for the most common small-matrix operation.

### C++ (Eigen fixed 4×4)

```cpp
#include <Eigen/Dense>

using Mat4 = Eigen::Matrix<double, 4, 4>;

Mat4 mul_fixed(const Mat4 &A, const Mat4 &B) {
    Mat4 C;
    C.noalias() = A * B;
    return C;
}
```

Expected: Eigen emits a fully-unrolled FMA (fused multiply-add) sequence using `vmulsd`/`vaddsd` or `vfmadd213sd`, with no loop overhead. At `-O3` or with `-march=haswell`, packed AVX FMA instructions may appear.

### C (naive triple loop)

```c
void mul_c(const double * restrict A, const double * restrict B,
           double * restrict C, int N) {
    for (int i = 0; i < N; ++i)
        for (int k = 0; k < N; ++k)
            for (int j = 0; j < N; ++j)
                C[i*N+j] += A[i*N+k] * B[k*N+j];
}
```

Expected: ikj loop order allows some vectorisation but still emits a loop with branch. GCC may unroll partially at `-O2`.

---

## Example 3 - Chained Expression A + B + C (Eigen Expression Templates)

**Goal:** show that Eigen evaluates `A + B + C` in a single pass with no temporary matrix.

### C++ (Eigen expression templates)

```cpp
#include <Eigen/Dense>

void add3_eigen(const Eigen::MatrixXd &A,
                const Eigen::MatrixXd &B,
                const Eigen::MatrixXd &C,
                Eigen::MatrixXd &Out) {
    Out = A + B + C;  // single traversal, no temporary
}
```

In assembly you will see a single loop body loading three values, adding them, and storing the result. No intermediate `malloc` or second loop appears.

### C equivalent (two passes)

```c
// Two-pass: first compute tmp = A+B, then out = tmp+C
void add3_c(const Matrix *a, const Matrix *b, const Matrix *c, Matrix *out) {
    size_t n = a->rows * a->cols;
    for (size_t i = 0; i < n; ++i)
        out->data[i] = a->data[i] + b->data[i] + c->data[i];
}
```

The C version in this project actually performs the addition in a single loop (`matrix_add3`), so it is competitive here. The difference only appears if you write it as two separate `matrix_add` calls.

---

## Example 4 - Fused Multiply-Add A * B + C

**Goal:** show `.noalias()` allows Eigen to skip an extra temporary.

### C++ with `.noalias()`

```cpp
#include <Eigen/Dense>

void mul_add_noalias(const Eigen::MatrixXd &A,
                     const Eigen::MatrixXd &B,
                     const Eigen::MatrixXd &C,
                     Eigen::MatrixXd &Out) {
    Out.noalias() = A * B + C;
}
```

Without `.noalias()`, Eigen must allocate a temporary result for `A * B` before adding `C` (to handle the aliasing case `Out = Out * B + Out`). With `.noalias()`, it can write directly into `Out` and then add `C` in place - one fewer allocation.

### C equivalent

```c
void mul_add_c(const Matrix *a, const Matrix *b,
               const Matrix *c, Matrix *out) {
    matrix_mul(a, b, out);          // out = A*B
    size_t n = out->rows * out->cols;
    for (size_t i = 0; i < n; ++i)
        out->data[i] += c->data[i]; // out += C
}
```

The C version is structurally identical to the `.noalias()` Eigen version for this operation.

---

## Example 5 - Equivalent C Runtime Loops for Comparison

Use this minimal self-contained snippet on Compiler Explorer to compare codegen directly:

```c
// C version: x86-64 GCC 13, flags: -O2 -std=c11
void matvec(int N, const double * restrict A,
            const double * restrict x, double * restrict y) {
    for (int i = 0; i < N; ++i) {
        double s = 0.0;
        for (int j = 0; j < N; ++j)
            s += A[i*N+j] * x[j];
        y[i] = s;
    }
}
```

Compare with the Eigen version:

```cpp
// C++ version: x86-64 GCC 13, flags: -O2 -std=c++17
#include <Eigen/Dense>

void matvec_eigen(const Eigen::MatrixXd &A,
                  const Eigen::VectorXd &x,
                  Eigen::VectorXd &y) {
    y.noalias() = A * x;
}
```

At `-O2`, the C loop is often auto-vectorised similarly to Eigen for matvec (both use SSE2 `mulpd`/`addpd`). The main difference appears at larger sizes where Eigen uses cache-blocking and explicit SIMD.

---

## Tips for Compiler Explorer

- Set **Compiler** to `x86-64 gcc 13.2` or `x86-64 clang 17`.
- Use flags `-O2 -std=c17` for C and `-O2 -std=c++17` for C++.
- Add `-march=native` or `-march=haswell` to enable AVX/FMA.
- Enable **"Demangle identifiers"** to make C++ symbols readable.
- Use **"Diff"** mode to compare C and C++ assembly side-by-side.
- Add `-fopt-info-vec` (GCC) or `-Rpass=loop-vectorize` (Clang) to see auto-vectorisation reports.
