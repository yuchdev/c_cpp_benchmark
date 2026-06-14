#pragma once
#include <Eigen/Dense>
#include <cstddef>

// Convenience aliases
using MatrixD  = Eigen::MatrixXd;                  // dynamic double
using VectorD  = Eigen::VectorXd;

template <int N>
using MatrixFixed = Eigen::Matrix<double, N, N>;

// Fill with deterministic pseudo-random values matching the C implementation
void eigen_fill_rand(MatrixD &m, unsigned int seed);

template <int N>
void eigen_fill_rand_fixed(MatrixFixed<N> &m, unsigned int seed) {
    unsigned int s = seed;
    for (int r = 0; r < N; ++r)
        for (int c = 0; c < N; ++c) {
            s = s * 1664525u + 1013904223u;
            m(r, c) = static_cast<double>(static_cast<int>(s >> 8) % 10000) * 0.0001;
        }
}
