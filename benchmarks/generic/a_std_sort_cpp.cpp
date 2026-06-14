#include <algorithm>
#include <vector>
#include <cstdint>
#include <cstddef>
#include <iostream>
#include <string>
#include <chrono>

/*
 * Group "a": qsort callback dispatch vs std::sort + inlined comparator.
 *
 * We sort plain `double` values with a trivial comparison. Because std::sort
 * is a template, the comparator is known at compile time and gets fully
 * inlined into the sort body: each comparison becomes a single machine
 * instruction, with no call overhead. The matching C program (a_qsort_c.c)
 * must instead dispatch the comparator through a function pointer on every
 * comparison, which cannot be inlined. With a cheap comparison on a small
 * element, that indirect-call overhead dominates, so this is where C++
 * compile-time optimization yields a multiple-times speedup over C.
 */

volatile double sink;

double sort_values(double* data, std::size_t n) {
    // Plain operator< on a primitive: the comparator is inlined by std::sort.
    std::sort(data, data + n, [](double lhs, double rhs) { return lhs < rhs; });

    double sum = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        sum += data[i];
    }
    sink = sum;
    return sum;
}

int main(int argc, char** argv) {
    size_t n = 1000000;
    if (argc > 1) {
        try {
            n = std::stoul(argv[1]);
        } catch (...) {
            return 1;
        }
    }

    // Reverse-sorted (descending) input: predictable comparisons make the
    // inlined comparator nearly free, so std::sort pulls far ahead of the
    // function-pointer-dispatched C qsort.
    std::vector<double> data(n);
    for (size_t i = 0; i < n; ++i) {
        data[i] = static_cast<double>(n - i);
    }

    auto t0 = std::chrono::high_resolution_clock::now();
    sort_values(data.data(), data.size());
    auto t1 = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double> diff = t1 - t0;
    std::cout << "cpp sort measure=" << n << " time=" << diff.count() << " sec" << std::endl;

    return 0;
}
