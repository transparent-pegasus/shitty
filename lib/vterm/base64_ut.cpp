/*
 * Copyright (C) 2026 Shitty team
 * MIT licensed
 * See the file LICENSE.MIT for the full license.
 */

#include "base64.h"

#include <std/tst/ut.h>
#include <std/str/view.h>
#include <std/ios/output.h>
#include <std/lib/buffer.h>

#include <cstring>

using namespace stl;

namespace {
    struct CaptureOutput final: public Output {
        size_t writeImpl(const void* data, size_t size) override;

        Buffer bytes;
    };

    static bool bytesEqual(const Buffer& buffer, StringView expected) {
        return StringView(buffer) == expected;
    }

    static bool decodeBufferInPlace(Buffer& buffer) {
        size_t size = buffer.used();
        if (!base64DecodeInPlace((u8*)buffer.mutData(), size)) {
            return false;
        }
        buffer.seekAbsolute(size);
        return true;
    }
}

size_t CaptureOutput::writeImpl(const void* data, size_t size) {
    bytes.append(data, size);
    return size;
}

STD_TEST_SUITE(Base64) {
    STD_TEST(EncodesKnownVectors) {
        Buffer output;

        STD_INSIST(bytesEqual(base64Encode(StringView(u8""), output), StringView(u8"")));
        STD_INSIST(bytesEqual(base64Encode(StringView(u8"f"), output), StringView(u8"Zg==")));
        STD_INSIST(bytesEqual(base64Encode(StringView(u8"fo"), output), StringView(u8"Zm8=")));
        STD_INSIST(bytesEqual(base64Encode(StringView(u8"foo"), output), StringView(u8"Zm9v")));
        STD_INSIST(bytesEqual(base64Encode(StringView(u8"foob"), output), StringView(u8"Zm9vYg==")));
        STD_INSIST(bytesEqual(base64Encode(StringView(u8"fooba"), output), StringView(u8"Zm9vYmE=")));
        STD_INSIST(bytesEqual(base64Encode(StringView(u8"foobar"), output), StringView(u8"Zm9vYmFy")));
    }

    STD_TEST(StreamsAcrossEveryInputBoundary) {
        u8 bytes[257];
        for (size_t index = 0; index != sizeof(bytes); ++index) {
            bytes[index] = (u8)index;
        }

        for (size_t length = 0; length <= sizeof(bytes); ++length) {
            Buffer expected;
            base64Encode(StringView(bytes, length), expected);
            for (size_t chunk = 1; chunk <= 17; ++chunk) {
                CaptureOutput output;
                Base64Encoder encoder;
                for (size_t offset = 0; offset < length; offset += chunk) {
                    const size_t remaining = length - offset;
                    const size_t current = remaining < chunk ? remaining : chunk;
                    encoder.write(output, StringView(bytes + offset, current));
                }
                encoder.finish(output);
                STD_INSIST(StringView(output.bytes) == StringView(expected));
            }
        }
    }

    STD_TEST(RoundTripsBinaryData) {
        const u8 bytes[] = {0, 1, 2, 0x7f, 0x80, 0xfe, 0xff};
        Buffer encoded;

        base64Encode(StringView(bytes, sizeof(bytes)), encoded);

        STD_INSIST(decodeBufferInPlace(encoded));
        STD_INSIST(encoded.used() == sizeof(bytes));
        STD_INSIST(StringView(encoded) == StringView(bytes, sizeof(bytes)));
    }

    STD_TEST(RoundTripsEveryByteAcrossBlockBoundaries) {
        u8 bytes[257];
        for (size_t index = 0; index < sizeof(bytes); ++index) {
            bytes[index] = (u8)index;
        }

        Buffer encoded;
        for (size_t length = 0; length <= sizeof(bytes); ++length) {
            const StringView input(bytes, length);
            base64Encode(input, encoded);
            STD_INSIST(decodeBufferInPlace(encoded));
            STD_INSIST(StringView(encoded) == input);
        }
    }

    STD_TEST(AcceptsCanonicalUnpaddedTail) {
        u8 one[] = u8"Zg";
        size_t oneSize = sizeof(one) - 1;
        STD_INSIST(base64DecodeInPlace(one, oneSize));
        STD_INSIST(StringView(one, oneSize) == StringView(u8"f"));

        u8 two[] = u8"Zm8";
        size_t twoSize = sizeof(two) - 1;
        STD_INSIST(base64DecodeInPlace(two, twoSize));
        STD_INSIST(StringView(two, twoSize) == StringView(u8"fo"));
    }

    STD_TEST(RejectsWhitespaceAndMalformedInput) {
        u8 whitespace[] = u8"Zm 9v";
        size_t whitespaceSize = sizeof(whitespace) - 1;
        STD_INSIST(!base64DecodeInPlace(whitespace, whitespaceSize));

        u8 truncated[] = u8"Z";
        size_t truncatedSize = sizeof(truncated) - 1;
        STD_INSIST(!base64DecodeInPlace(truncated, truncatedSize));

        u8 invalid[] = u8"Zm9*";
        size_t invalidSize = sizeof(invalid) - 1;
        STD_INSIST(!base64DecodeInPlace(invalid, invalidSize));
    }

    STD_TEST(RejectsBadPaddingAndNonCanonicalTails) {
        const StringView malformed[] = {
            StringView(u8"="),
            StringView(u8"===="),
            StringView(u8"Zg="),
            StringView(u8"Zg==="),
            StringView(u8"Zm=8"),
            StringView(u8"Zm8=="),
            StringView(u8"Zg==Zg=="),
            StringView(u8"Zh=="),
            StringView(u8"Zm9="),
            StringView(u8"Zh"),
            StringView(u8"Zm9"),
        };

        for (const StringView input : malformed) {
            u8 bytes[16];
            memcpy(bytes, input.data(), input.length());
            size_t size = input.length();
            STD_INSIST(!base64DecodeInPlace(bytes, size));
        }
    }

    STD_TEST(EncodeReturnsCallerBuffer) {
        Buffer output;

        STD_INSIST(&base64Encode(StringView(u8"x"), output) == &output);
    }

    STD_TEST(DecodesInPlace) {
        u8 bytes[] = u8"AAECA3+A/v8=";
        const u8 expected[] = {0, 1, 2, 3, 0x7f, 0x80, 0xfe, 0xff};
        size_t size = sizeof(bytes) - 1;

        STD_INSIST(base64DecodeInPlace(bytes, size));
        STD_INSIST(StringView(bytes, size) == StringView(expected, sizeof(expected)));
    }

    STD_TEST(KittyVectorsAcceptPaddedAndUnpaddedInput) {
        struct Vector {
            StringView padded;
            StringView unpadded;
            StringView plain;
        };

        const Vector vectors[] = {
            {StringView(u8"bGlnaHQgdw=="), StringView(u8"bGlnaHQgdw"), StringView(u8"light w")},
            {StringView(u8"bGlnaHQgd28="), StringView(u8"bGlnaHQgd28"), StringView(u8"light wo")},
            {StringView(u8"bGlnaHQgd29y"), StringView(u8"bGlnaHQgd29y"), StringView(u8"light wor")},
        };

        for (const Vector& vector : vectors) {
            Buffer encoded;
            STD_INSIST(base64Encode(vector.plain, encoded) == vector.padded);

            u8 bytes[16];
            memcpy(bytes, vector.padded.data(), vector.padded.length());
            size_t size = vector.padded.length();
            STD_INSIST(base64DecodeInPlace(bytes, size));
            STD_INSIST(StringView(bytes, size) == vector.plain);

            memcpy(bytes, vector.unpadded.data(), vector.unpadded.length());
            size = vector.unpadded.length();
            STD_INSIST(base64DecodeInPlace(bytes, size));
            STD_INSIST(StringView(bytes, size) == vector.plain);
        }
    }
}
