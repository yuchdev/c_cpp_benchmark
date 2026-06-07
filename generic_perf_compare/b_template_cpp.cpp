#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>
#include <chrono>

/*
 * Group "b": function-pointer callback dispatch vs template/lambda transform.
 *
 * We transform a large buffer element-by-element, exactly like the C program
 * (b_callback_c.c). The difference is *how* the operation is dispatched.
 *
 * Here apply_transform is a template parameterized on the callable type, and
 * the operation is supplied as a lambda. Because the concrete operation is
 * known at compile time, the compiler inlines it directly into the loop body
 * (no call at all) and is then free to auto-vectorize the transform.
 *
 * The C version, by contrast, can only reach its operation through a function
 * pointer whose target is opaque to the optimizer, so it pays a real,
 * non-inlinable indirect call for every element. That per-element dispatch
 * overhead accumulates over the whole buffer, which is why this compile-time
 * optimization lets C++ run several times faster than the C callback.
 */

template <class F>
void apply_transform(int* out, const int* in, std::size_t n, F op) {
    for (std::size_t i = 0; i < n; ++i) {
        out[i] = op(in[i]);
    }
}

volatile long long sink;

int main(int argc, char** argv) {
    std::size_t n = 10000000;
    if (argc > 1) {
        try {
            long v = std::stol(argv[1]);
            if (v <= 0) return 1;
            n = static_cast<std::size_t>(v);
        } catch (...) {
            return 1;
        }
    }

    std::vector<int> in(n);
    std::vector<int> out(n);
    for (std::size_t i = 0; i < n; ++i) {
        in[i] = static_cast<int>(i & 0xff);
    }

    // The lambda is a compile-time-known callable: std::sort-style inlining
    // collapses the per-element dispatch to nothing and vectorizes the loop.
    auto t0 = std::chrono::high_resolution_clock::now();
    apply_transform(out.data(), in.data(), n, [](int x) { return x * 3 + 1; });
    auto t1 = std::chrono::high_resolution_clock::now();

    long long sum = 0;
    for (std::size_t i = 0; i < n; ++i) {
        sum += out[i];
    }
    sink = sum;

    std::chrono::duration<double> diff = t1 - t0;
    std::cout << "cpp template measure=" << n << " time=" << diff.count() << " sec" << std::endl;

    return 0;
}
