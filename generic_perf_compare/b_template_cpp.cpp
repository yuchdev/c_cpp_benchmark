#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <chrono>

template <class F>
void apply_transform(int* out, const int* in, std::size_t n, F op) {
    for (std::size_t i = 0; i < n; ++i) {
        out[i] = op(in[i]);
    }
}

volatile int sink;

int run_apply(const int* in, std::size_t n) {
    int out[64];
    if (n > 64) n = 64;

    apply_transform(out, in, n, [](int x) {
        return x * 3 + 1;
    });

    int sum = 0;
    for (std::size_t i = 0; i < n; ++i) {
        sum += out[i];
    }
    sink = sum;
    return sum;
}

int main(int argc, char** argv) {
    int iters = 10000000;
    if (argc > 1) {
        try {
            iters = std::stoi(argv[1]);
        } catch (...) {
            return 1;
        }
    }

    int in[8] = {1,2,3,4,5,6,7,8};

    auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iters; ++i) {
        run_apply(in, 8);
    }
    auto t1 = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double> diff = t1 - t0;
    std::cout << "cpp template measure=" << iters << " time=" << diff.count() << " sec" << std::endl;

    return 0;
}
