/*
 * Copyright (C) 2026 Shitty team
 * MIT licensed
 * See the file LICENSE.MIT for the full license.
 */

#include "utf8.h"

#include <std/tst/ut.h>
#include <std/str/view.h>
#include <std/lib/buffer.h>

using namespace stl;

namespace {
    static bool bytesEqual(const Buffer& buffer, StringView expected) {
        return StringView(buffer) == expected;
    }

    static Buffer& encodeCodepoint(u32 codepoint, Buffer& output) {
        output.reset();
        Utf8Encoder::pushUnicode(codepoint, [&](u8 byte) {
            output.append(&byte, 1);
        });
        return output;
    }
}

STD_TEST_SUITE(Utf8) {
    STD_TEST(EncoderCoversEveryLength) {
        Buffer output;

        STD_INSIST(bytesEqual(encodeCodepoint(0x24, output), StringView(u8"\x24")));
        STD_INSIST(bytesEqual(encodeCodepoint(0xa2, output), StringView(u8"\xc2\xa2")));
        STD_INSIST(bytesEqual(encodeCodepoint(0x20ac, output), StringView(u8"\xe2\x82\xac")));
        STD_INSIST(bytesEqual(encodeCodepoint(0x1f642, output), StringView(u8"\xf0\x9f\x99\x82")));
    }

    STD_TEST(DecoderCompletesMultibyteSequences) {
        Utf8Decoder decoder;

        STD_INSIST(decoder.pushByte(0xe2) == 0);
        STD_INSIST(decoder.expectsContinuation());
        STD_INSIST(decoder.pushByte(0x82) == 0);
        STD_INSIST(decoder.pushByte(0xac) == 1);
        STD_INSIST(!decoder.expectsContinuation());
        STD_INSIST(decoder.getUnicode() == 0x20ac);
    }

    STD_TEST(DecoderReadsOneBoundedScalar) {
        u32 codepoint = 0;
        static constexpr u8 ascii[] = {'A'};
        static constexpr u8 cent[] = {0xc2, 0xa2};
        static constexpr u8 euro[] = {0xe2, 0x82, 0xac};
        static constexpr u8 smile[] = {0xf0, 0x9f, 0x99, 0x82};

        STD_INSIST(Utf8Decoder::decodeOne(ascii, sizeof(ascii), codepoint) == 1);
        STD_INSIST(codepoint == 'A');
        STD_INSIST(Utf8Decoder::decodeOne(cent, sizeof(cent), codepoint) == 2);
        STD_INSIST(codepoint == 0xa2);
        STD_INSIST(Utf8Decoder::decodeOne(euro, sizeof(euro), codepoint) == 3);
        STD_INSIST(codepoint == 0x20ac);
        STD_INSIST(Utf8Decoder::decodeOne(smile, sizeof(smile), codepoint) == 4);
        STD_INSIST(codepoint == 0x1f642);
    }

    STD_TEST(BoundedDecoderRejectsInvalidAndTruncatedSequences) {
        u32 codepoint = 1;
        static constexpr u8 truncated[] = {0xf0, 0x9f, 0x99};
        static constexpr u8 overlong[] = {0xe0, 0x80, 0x80};
        static constexpr u8 surrogate[] = {0xed, 0xa0, 0x80};
        static constexpr u8 outOfRange[] = {0xf4, 0x90, 0x80, 0x80};
        static constexpr u8 stray[] = {0x80};

        STD_INSIST(Utf8Decoder::decodeOne(truncated, sizeof(truncated), codepoint) == 0);
        STD_INSIST(Utf8Decoder::decodeOne(overlong, sizeof(overlong), codepoint) == 0);
        STD_INSIST(Utf8Decoder::decodeOne(surrogate, sizeof(surrogate), codepoint) == 0);
        STD_INSIST(Utf8Decoder::decodeOne(outOfRange, sizeof(outOfRange), codepoint) == 0);
        STD_INSIST(Utf8Decoder::decodeOne(stray, sizeof(stray), codepoint) == 0);
    }

    STD_TEST(DecoderRejectsOverlongSurrogateAndOutOfRangeSequences) {
        Utf8Decoder decoder;

        STD_INSIST(decoder.pushByte(0xe0) == 0);
        STD_INSIST(decoder.pushByte(0x80) == 2);
        STD_INSIST(decoder.pushByte(0x80) == 1);
        STD_INSIST(decoder.getUnicode() == Unicode_Replacement_Character);

        STD_INSIST(decoder.pushByte(0xed) == 0);
        STD_INSIST(decoder.pushByte(0xa0) == 2);
        STD_INSIST(decoder.pushByte(0x80) == 1);
        STD_INSIST(decoder.getUnicode() == Unicode_Replacement_Character);

        STD_INSIST(decoder.pushByte(0xf4) == 0);
        STD_INSIST(decoder.pushByte(0x90) == 2);
        STD_INSIST(decoder.pushByte(0x80) == 1);
        STD_INSIST(decoder.pushByte(0x80) == 1);
        STD_INSIST(decoder.getUnicode() == Unicode_Replacement_Character);
    }

    STD_TEST(DecoderReportsStrayAndInterruptedContinuation) {
        Utf8Decoder decoder;

        STD_INSIST(decoder.pushByte(0x80) == 1);
        STD_INSIST(decoder.getUnicode() == Unicode_Replacement_Character);

        STD_INSIST(decoder.pushByte(0xe2) == 0);
        STD_INSIST(decoder.pushByte(0x41) == 2);
        STD_INSIST(decoder.getUnicode() == Unicode_Replacement_Character);
        STD_INSIST(!decoder.expectsContinuation());
    }

    STD_TEST(DecoderFlushesPrematureEndOnce) {
        Utf8Decoder decoder;
        decoder.pushByte(0xf0);
        decoder.pushByte(0x9f);

        STD_INSIST(decoder.checkPrematureEOS());
        STD_INSIST(decoder.getUnicode() == Unicode_Replacement_Character);
        STD_INSIST(!decoder.checkPrematureEOS());
    }

    STD_TEST(DecoderResetAndDirectUnicode) {
        Utf8Decoder decoder;
        decoder.pushByte(0xe2);
        decoder.reset();

        STD_INSIST(!decoder.expectsContinuation());
        STD_INSIST(decoder.getUnicode() == 0);
        STD_INSIST(!decoder.onUnicode(0));
        STD_INSIST(decoder.onUnicode(0x1f642));
        STD_INSIST(decoder.getUnicode() == 0x1f642);
    }
}
