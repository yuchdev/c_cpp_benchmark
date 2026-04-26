#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <chrono>

constexpr std::uint32_t mix32(std::uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

constexpr std::array<std::uint32_t, 256> make_table() {
    std::array<std::uint32_t, 256> table{};
    for (std::size_t i = 0; i < table.size(); ++i) {
        table[i] = mix32(static_cast<std::uint32_t>(i));
    }
    return table;
}

constexpr auto table = make_table();
volatile std::uint32_t sink;

std::uint32_t use_table(const std::uint8_t* data, std::size_t n) {
    std::uint32_t acc = 0;
    for (std::size_t i = 0; i < n; ++i) {
        acc ^= table[data[i]];
    }
    sink = acc;
    return acc;
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

    uint8_t data[8] = {1, 7, 42, 255, 3, 9, 11, 19};

    auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iters; ++i) {
        use_table(data, 8);
    }
    auto t1 = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double> diff = t1 - t0;
    std::cout << "cpp constexpr_table measure=" << iters << " time=" << diff.count() << " sec" << std::endl;

    return 0;
}
