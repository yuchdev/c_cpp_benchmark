#include <algorithm>
#include <vector>
#include <cstdint>
#include <cstddef>
#include <iostream>
#include <string>
#include <chrono>

struct Order {
    std::uint64_t ts;
    std::uint32_t id;
    std::uint32_t qty;
};

volatile std::uint64_t sink;

std::uint64_t sort_orders(Order* data, std::size_t n) {
    std::sort(data, data + n, [](const Order& lhs, const Order& rhs) {
        if (lhs.ts != rhs.ts) return lhs.ts < rhs.ts;
        return lhs.id < rhs.id;
    });

    std::uint64_t sum = 0;
    for (std::size_t i = 0; i < n; ++i) {
        sum += data[i].ts;
        sum += data[i].id;
        sum += data[i].qty;
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

    std::vector<Order> data(n);
    for (size_t i = 0; i < n; ++i) {
        data[i] = { (uint64_t)(n - i), (uint32_t)i, (uint32_t)(i * 10) };
    }

    auto t0 = std::chrono::high_resolution_clock::now();
    sort_orders(data.data(), data.size());
    auto t1 = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double> diff = t1 - t0;
    std::cout << "cpp sort measure=" << n << " time=" << diff.count() << " sec" << std::endl;

    return 0;
}
