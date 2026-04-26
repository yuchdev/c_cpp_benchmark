#define _POSIX_C_SOURCE 200809L
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <ctime>
#include <iostream>
#include <string>
#include <chrono>

#if defined(_MSC_VER)
#define NOINLINE __declspec(noinline)
#else
#define NOINLINE __attribute__((noinline))
#endif

struct Vec4 {
    float x, y, z, w;

    constexpr Vec4(float x_=0, float y_=0, float z_=0, float w_=0) noexcept : x(x_), y(y_), z(z_), w(w_) {}

    friend inline Vec4 operator+(const Vec4& a, const Vec4& b) noexcept {
        return {a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w};
    }

    friend inline Vec4 operator*(const Vec4& a, float s) noexcept {
        return {a.x * s, a.y * s, a.z * s, a.w * s};
    }

    inline float dot(const Vec4& other) const noexcept {
        return x * other.x + y * other.y + z * other.z + w * other.w;
    }
};

NOINLINE static float kernel(int iters) {
    Vec4 a{1.0f, 2.0f, 3.0f, 4.0f};
    Vec4 b{5.0f, 6.0f, 7.0f, 8.0f};
    Vec4 acc{0.25f, 0.5f, 0.75f, 1.0f};
    float sink = 0.0f;

    for (int i = 0; i < iters; ++i) {
        Vec4 c = a + b;
        Vec4 d = c * (0.00001f * (1.0f + static_cast<float>(i & 7)));
        sink += d.dot(acc);
        acc = acc + d;
        a = a + (d * 0.25f);
        b = b + (d * 0.125f);
    }

    volatile float guard = sink + acc.x + acc.y + acc.z + acc.w;
    return guard;
}

static double now_sec() {
    timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<double>(ts.tv_sec) + static_cast<double>(ts.tv_nsec) / 1e9;
}

int main(int argc, char** argv) {
    int iters = 2000000;
    if (argc > 1) {
        try {
            iters = std::stoi(argv[1]);
        } catch (...) {
            return 1;
        }
    }

    auto t0 = std::chrono::high_resolution_clock::now();
    float result = kernel(iters);
    auto t1 = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double> diff = t1 - t0;
    std::cout << "cpp class_operator measure=" << iters << " time=" << diff.count() << " sec" << std::endl;

    return 0;
}
