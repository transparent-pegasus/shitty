/*
 * Copyright (C) 2026 Shitty team
 * MIT licensed
 * See the file LICENSE.MIT for the full license.
 */

#pragma once

#include <std/sys/types.h>

struct Hex {
    u64 value;
    u8 width = 0;
    bool uppercase = false;
};
