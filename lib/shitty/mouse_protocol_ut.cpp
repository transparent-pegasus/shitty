/*
 * Copyright (C) 2026 Shitty team
 * MIT licensed
 * See the file LICENSE.MIT for the full license.
 */

#include <lib/vterm/mouse_protocol.h>

#include <std/tst/ut.h>
#include <std/str/view.h>
#include <std/lib/buffer.h>
#include <std/str/builder.h>

using namespace stl;

namespace {
    static void encodeInto(Buffer& out, MouseTrackingEnc encoding, MouseEventType type, unsigned modifiers, int motionButton, int button, int column, int row) {
        StringBuilder output;
        STD_INSIST(encodeMouseProtocol(output, encoding, type, modifiers, motionButton, button, column, row));
        out.xchg(output);
    }

    static bool encodesAs(StringView expected, MouseTrackingEnc encoding, MouseEventType type, unsigned modifiers, int motionButton, int button, int column, int row) {
        Buffer output;
        encodeInto(output, encoding, type, modifiers, motionButton, button, column, row);
        return StringView(output) == expected;
    }
}

STD_TEST_SUITE(MouseProtocol) {
    STD_TEST(EncodesLegacyPressAndRelease) {
        STD_INSIST(encodesAs(StringView(u8"\x1b[M !!"), MouseTrackingEnc::Default, MouseEventType::Press, 0, 0, 1, 1, 1));
        STD_INSIST(encodesAs(StringView(u8"\x1b[M#!!"), MouseTrackingEnc::Default, MouseEventType::Release, 0, 0, 1, 1, 1));
    }

    STD_TEST(EncodesSgrPressReleaseAndMotion) {
        STD_INSIST(encodesAs(StringView(u8"\x1b[<16;10;20M"), MouseTrackingEnc::SGR, MouseEventType::Press, MouseControl, 0, 1, 10, 20));
        STD_INSIST(encodesAs(StringView(u8"\x1b[<0;10;20m"), MouseTrackingEnc::SGR, MouseEventType::Release, 0, 0, 1, 10, 20));
        STD_INSIST(encodesAs(StringView(u8"\x1b[<0;236;120m"), MouseTrackingEnc::SGRPixels, MouseEventType::Release, 0, 0, 1, 236, 120));
        STD_INSIST(encodesAs(StringView(u8"\x1b[<37;10;20M"), MouseTrackingEnc::SGR, MouseEventType::Motion, MouseShift, 2, 0, 10, 20));
    }

    STD_TEST(EncodesWheelAndExtendedButtons) {
        STD_INSIST(encodesAs(StringView(u8"\x1b[<64;2;3M"), MouseTrackingEnc::SGR, MouseEventType::Press, 0, 0, 4, 2, 3));
        STD_INSIST(encodesAs(StringView(u8"\x1b[<131;2;3M"), MouseTrackingEnc::SGR, MouseEventType::Press, 0, 0, 11, 2, 3));
        STD_INSIST(encodesAs(StringView(u8"\x1b[100;2;3M"), MouseTrackingEnc::URXVT, MouseEventType::Press, MouseShift, 0, 4, 2, 3));
    }

    STD_TEST(ClampsLegacyCoordinates) {
        Buffer low;
        Buffer high;
        encodeInto(low, MouseTrackingEnc::Default, MouseEventType::Press, 0, 0, 1, -100, -100);
        encodeInto(high, MouseTrackingEnc::Default, MouseEventType::Press, 0, 0, 1, 1000, 1000);

        STD_INSIST(StringView(low) == StringView(u8"\x1b[M !!"));
        STD_INSIST(((const u8*)(high.data()))[4] == 255);
        STD_INSIST(((const u8*)(high.data()))[5] == 255);
    }

    STD_TEST(UsesUtf8ForLargeCoordinates) {
        Buffer output;
        encodeInto(output, MouseTrackingEnc::UTF8, MouseEventType::Press, 0, 0, 1, 300, 400);

        const u8* bytes = (const u8*)(output.data());
        STD_INSIST(output.used() == 8);
        STD_INSIST(StringView(bytes, (size_t)3) == StringView(u8"\x1b[M"));
        STD_INSIST(bytes[3] == 32);
        STD_INSIST(bytes[4] == 0xc5);
        STD_INSIST(bytes[5] == 0x8c);
    }

    STD_TEST(RejectsUnknownButtonsWithoutWriting) {
        StringBuilder output;
        output << StringView(u8"prefix");

        STD_INSIST(!encodeMouseProtocol(output, MouseTrackingEnc::SGR, MouseEventType::Press, 0, 0, 0, 1, 1));
        STD_INSIST(StringView(output) == StringView(u8"prefix"));
    }
}
