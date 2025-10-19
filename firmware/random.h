/*
 * Copyright (c) 2025 Przemysław Romaniak
 * 
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the root directory for details.
*/

#pragma once

#include <cstdint>

namespace RAND {
    void init();
    uint32_t next();
}