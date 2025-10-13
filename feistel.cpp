/*
 * Copyright (c) 2025 Przemysław Romaniak
 * 
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the root directory for details.
*/

#include <cstdint>
#include <bit>

// Helpers (C++20)
static inline uint32_t next_power_of_two(uint32_t n) {
    return n <= 1u ? 1u : std::bit_ceil(n);
}

static inline uint32_t round_function(uint32_t v, uint32_t rk) {
    uint32_t z = v + rk;
    z ^= z >> 16;
    z *= 0x7FEB352Du;
    z ^= z >> 15;
    z *= 0x846CA68Bu;
    z ^= z >> 16;
    return z;
}

// Safe mask builder
static inline uint32_t mask_bits(unsigned bits) {
    return bits == 32 ? 0xFFFFFFFFu : (bits ? uint32_t((1ull << bits) - 1ull) : 0u);
}

// Feistel on r bits (1 <= r <= 32)
static inline uint32_t feistel_rbits(uint32_t x, unsigned r, uint32_t key, int rounds = 3) {
    unsigned Lbits = r / 2u;
    unsigned Rbits = r - Lbits;

    uint32_t maskL = mask_bits(Lbits);
    uint32_t maskR = mask_bits(Rbits);

    uint32_t L = (x >> Rbits) & maskL;
    uint32_t R = x & maskR;

    for (int round = 0; round < rounds; ++round) {
        // round key; golden-ratio stride helps decorrelate rounds
        uint32_t rk = key ^ (uint32_t)(round * 0x9E3779B1u);

        uint32_t F = round_function(R, rk) & maskL;
        uint32_t newL = R;              // already within maskR width
        uint32_t newR = (L ^ F) & maskL;

        L = newL;
        R = newR;

        // swap widths and masks
        unsigned tmpb = Lbits; Lbits = Rbits; Rbits = tmpb;
        uint32_t tmpm = maskL; maskL = maskR; maskR = tmpm;
    }

    uint32_t result = (L << Rbits) | R;
    if (r < 32) result &= mask_bits(r);
    return result;
}

// Permutation 0..N-1 using cycle-walking
uint32_t permute(uint32_t x, uint32_t N, uint32_t key, int rounds) {
    if (N == 1u) return 0u;

    uint32_t M = std::bit_ceil(N);
    unsigned r = std::bit_width(M) - 1u;

    uint32_t y = x;
    do {
        y = feistel_rbits(y, r, key, rounds);
    } while (y >= N);

    return y;
}