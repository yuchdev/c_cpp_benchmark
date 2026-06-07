#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <chrono>

/*
 * Group "e" benchmark: runtime lookup-table transform vs compile-time constexpr
 * table fusion. (C++ side -- the compile-time version.)
 *
 * Same real-world scenario as the C version (e_runtime_table_c.c): a byte
 * substitution pipeline applies ROUNDS rounds of (S-box substitution + bit
 * rotation) to every byte of a large buffer.
 *
 * The key difference is purely compile-time vs runtime: here the S-box AND the
 * full ROUNDS-round pipeline are `constexpr`, so the compiler evaluates the
 * entire pipeline at *build time* and FUSES all ROUNDS rounds into a single
 * 256-entry table baked into .rodata. At run time the hot loop performs exactly
 * ONE table lookup per byte and auto-vectorizes the reduction, whereas the C
 * version must perform ROUNDS serial, data-dependent lookups per byte because
 * its S-box is only known at run time.
 *
 * Both programs use the identical seed, round function, round count and input
 * fill, so they compute the identical result -- only WHEN the work happens
 * (compile time vs run time) differs.
 */

#if defined(_MSC_VER)
#define NOINLINE __declspec(noinline)
#else
#define NOINLINE __attribute__((noinline))
#endif

/* Must match the C version exactly. */
#ifndef ROUNDS
#define ROUNDS 24
#endif
constexpr int ROUND_ROT = 3;
constexpr std::uint32_t SEED = 0x9E3779B9u;

constexpr std::uint32_t mix32(std::uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

constexpr std::uint8_t rotl8(std::uint8_t x, int r) {
    return static_cast<std::uint8_t>((x << r) | (x >> (8 - r)));
}

/* The same S-box the C version builds at run time -- here at compile time. */
constexpr std::array<std::uint8_t, 256> make_sbox() {
    std::array<std::uint8_t, 256> sbox{};
    for (std::size_t i = 0; i < sbox.size(); ++i) {
        sbox[i] = static_cast<std::uint8_t>(mix32(static_cast<std::uint32_t>(i) ^ SEED) & 0xFFu);
    }
    return sbox;
}

/*
 * Compile-time fusion: run the whole ROUNDS-round pipeline for every possible
 * input byte and collapse it into one 256-entry table. This is the work the C
 * version repeats for every byte at run time.
 */
constexpr std::array<std::uint8_t, 256> make_fused_table() {
    constexpr auto sbox = make_sbox();
    std::array<std::uint8_t, 256> fused{};
    for (std::size_t i = 0; i < fused.size(); ++i) {
        std::uint8_t x = static_cast<std::uint8_t>(i);
        for (int r = 0; r < ROUNDS; ++r) {
            x = sbox[x];
            x = rotl8(x, ROUND_ROT);
        }
        fused[i] = x;
    }
    return fused;
}

/* One lookup per byte at run time; the ROUNDS rounds are already folded in. */
constexpr auto fused = make_fused_table();
volatile std::uint32_t sink;

NOINLINE static std::uint32_t transform_buffer(const std::uint8_t* __restrict data,
                                               std::size_t n) {
    std::uint32_t acc = 0;
    for (std::size_t i = 0; i < n; ++i) {
        acc ^= static_cast<std::uint32_t>(fused[data[i]]); /* single fused lookup */
    }
    return acc;
}

int main(int argc, char** argv) {
    std::size_t n = 16777216; /* default dataset: 16 MiB of bytes */
    if (argc > 1) {
        try {
            long long v = std::stoll(argv[1]);
            if (v <= 0) return 1;
            n = static_cast<std::size_t>(v);
        } catch (...) {
            return 1;
        }
    }

    auto* data = new std::uint8_t[n];
    /* Deterministic fill -- byte-for-byte the same closed form as the C version. */
    for (std::size_t i = 0; i < n; ++i) {
        data[i] = static_cast<std::uint8_t>(static_cast<std::uint32_t>((i * 2654435761u) >> 24) & 0xFFu);
    }

    auto t0 = std::chrono::high_resolution_clock::now();
    std::uint32_t acc = transform_buffer(data, n);
    auto t1 = std::chrono::high_resolution_clock::now();

    sink = acc; /* store result so the loop is not elided */
    std::chrono::duration<double> diff = t1 - t0;
    std::cout << "cpp constexpr_table measure=" << n
              << " time=" << diff.count() << " sec" << std::endl;

    delete[] data;
    return 0;
}
