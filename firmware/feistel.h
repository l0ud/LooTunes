/*
 * Copyright (c) 2025 Przemysław Romaniak
 * 
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the root directory for details.
*/

#pragma once

#include <cstdint>

uint32_t permute(uint32_t x, uint32_t N, uint32_t key = 0xDEADBEEF, int rounds = 3);
