/*
 * Copyright (c) 2025 Przemysław Romaniak
 * 
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the root directory for details.
*/

#pragma once

#include <array>

template <typename T, std::size_t N, T Value>
constexpr auto make_filled_array() {
    std::array<T, N> arr{};
    arr.fill(Value);
    return arr;
}

inline __attribute__((always_inline)) UINT freadwrap(void* buff, UINT size) {
    UINT br;
    FRESULT res = pf_read_cached(buff, size, &br);
    if (res != FR_OK) {
        // Handle read error
        while(1) { };
    }

    return br;
}
