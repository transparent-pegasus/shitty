/*
 * Copyright (C) 2026 Shitty team
 * MIT licensed
 * See the file LICENSE.MIT for the full license.
 */

#include "num.h"

#include <std/tst/ut.h>
#include <std/str/view.h>

#include <stdlib.h>

using namespace stl;

STD_TEST_SUITE(Num) {
    STD_TEST(SignedParse) {
        i64 value = 7;

        STD_INSIST(parseI64(StringView(u8"0"), value) && value == 0);
        STD_INSIST(parseI64(StringView(u8"42"), value) && value == 42);
        STD_INSIST(parseI64(StringView(u8"-42"), value) && value == -42);
        STD_INSIST(parseI64(StringView(u8"+42"), value) && value == 42);
        STD_INSIST(parseI64(StringView(u8"9223372036854775807"), value) && value == 9223372036854775807ll);
        STD_INSIST(parseI64(StringView(u8"-9223372036854775808"), value) && value == (-9223372036854775807ll - 1));
        STD_INSIST(!parseI64(StringView(u8"9223372036854775808"), value));
        STD_INSIST(!parseI64(StringView(), value));
        STD_INSIST(!parseI64(StringView(u8"-"), value));
        STD_INSIST(!parseI64(StringView(u8"12x"), value));
        STD_INSIST(!parseI64(StringView(u8" 12"), value));
    }

    STD_TEST(UnsignedParse) {
        u64 value = 7;

        STD_INSIST(parseU64(StringView(u8"18446744073709551615"), value) && value == ~(u64)(0));
        STD_INSIST(!parseU64(StringView(u8"18446744073709551616"), value));
        STD_INSIST(parseU64(StringView(u8"ff"), value, 16) && value == 0xff);
        STD_INSIST(parseU64(StringView(u8"777"), value, 8) && value == 0777);
        STD_INSIST(parseU64(StringView(u8"101"), value, 2) && value == 5);
        STD_INSIST(!parseU64(StringView(u8"-1"), value));
        STD_INSIST(!parseU64(StringView(u8"8"), value, 8));
    }

    STD_TEST(FloatParse) {
        double value = 7.0;

        STD_INSIST(parseF64(StringView(u8"1.5"), value) && value == 1.5);
        STD_INSIST(parseF64(StringView(u8"-2.5e2"), value) && value == -250.0);
        STD_INSIST(parseF64(StringView(u8"0"), value) && value == 0.0);
        STD_INSIST(parseF64(StringView(u8".5"), value) && value == 0.5);
        STD_INSIST(parseF64(StringView(u8"1e22"), value) && value == 1e22);
        STD_INSIST(parseF64(StringView(u8"inf"), value) && value > 1.7e308);
        STD_INSIST(parseF64(StringView(u8"nan"), value) && value != value);
        STD_INSIST(!parseF64(StringView(u8"1.5x"), value));
        STD_INSIST(!parseF64(StringView(u8"."), value));
        STD_INSIST(!parseF64(StringView(), value));
    }

    STD_TEST(FloatRoundtrip) {
        const double values[] = {0.0, 1.0, -0.01, 3.141592653589793, 6.626e-34, 5e+22, -1.7976931348623157e308};

        for (const double value : values) {
            char buf[32];

            *formatF64Roundtrip(value, buf) = 0;
            STD_INSIST(strtod(buf, nullptr) == value);
        }
    }
}
