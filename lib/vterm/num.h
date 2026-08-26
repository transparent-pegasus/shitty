/*
 * Copyright (C) 2026 Shitty team
 * MIT licensed
 * See the file LICENSE.MIT for the full license.
 */

#pragma once

#include <std/sys/types.h>

namespace stl {
    class StringView;
}

// Whole-view numeric parses: an optional sign, then digits of the base,
// nothing else. Empty input, stray bytes, and overflow all report false
// and leave out untouched. parseF64 additionally accepts inf, infinity,
// and nan in any case, and never consults the locale.
bool parseI64(stl::StringView text, i64& out);
bool parseU64(stl::StringView text, u64& out, unsigned base = 10);
bool parseF64(stl::StringView text, double& out);

// Enough digits to reparse the exact same double; needs 32 bytes.
// Returns the end of the written text.
char* formatF64Roundtrip(double value, char* buf);
