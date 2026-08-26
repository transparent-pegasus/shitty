/*
 * Copyright (C) 2026 Shitty team
 * MIT licensed
 * See the file LICENSE.MIT for the full license.
 */

#include "color_spec.h"
#include "color_names.h"

#include <std/tst/ut.h>
#include <std/str/view.h>

using namespace stl;

namespace {
    static bool close(u8 value, u8 expected, u8 tolerance = 1) {
        return value >= expected - tolerance && value <= expected + tolerance;
    }
}

STD_TEST_SUITE(ColorSpec) {
    STD_TEST(ResolvesX11ColorNamesCaseInsensitively) {
        Color color{};

        STD_INSIST(colorFromName(StringView(u8"teal"), color));
        STD_INSIST((color == Color{0x00, 0x80, 0x80}));
        STD_INSIST(colorFromName(StringView(u8"AliceBlue"), color));
        STD_INSIST((color == Color{0xf0, 0xf8, 0xff}));
        STD_INSIST(!colorFromName(StringView(u8"not a color"), color));
    }

    STD_TEST(ConvertsLinearRgbIntensity) {
        Color color{};

        STD_INSIST(colorFromRgbIntensity(0.0, 0.5, 1.0, color));
        STD_INSIST(color.red == 0);
        STD_INSIST(close(color.green, 188));
        STD_INSIST(color.blue == 255);
    }

    STD_TEST(ConvertsCieXyzRed) {
        Color color{};

        STD_INSIST(colorFromCieXyz(0.4124564, 0.2126729, 0.0193339, color));
        STD_INSIST(close(color.red, 255));
        STD_INSIST(close(color.green, 0));
        STD_INSIST(close(color.blue, 0));
    }

    STD_TEST(ConvertsLabWhiteAndBlack) {
        Color color{};

        STD_INSIST(colorFromCieLab(100.0, 0.0, 0.0, color));
        STD_INSIST(close(color.red, 255));
        STD_INSIST(close(color.green, 255));
        STD_INSIST(close(color.blue, 255));

        STD_INSIST(colorFromCieLab(0.0, 0.0, 0.0, color));
        STD_INSIST((color == Color{0, 0, 0}));
    }

    STD_TEST(ScalesScientificNotation) {
        double positive = 0.0;
        double negative = 0.0;

        STD_INSIST(finishColorNumber(2.5, false, 1, true, positive));
        STD_INSIST(finishColorNumber(2.5, true, 1, false, negative));
        STD_INSIST(positive == 0.25);
        STD_INSIST(negative == -25.0);
    }

    STD_TEST(RejectsInvalidConvertedRanges) {
        Color color{};

        STD_INSIST(!colorFromRgbIntensity(-0.1, 0.0, 0.0, color));
        STD_INSIST(!colorFromRgbIntensity(0.0, 0.0, 1.1, color));
        STD_INSIST(!colorFromCieXyz(0.0, 1.1, 0.0, color));
        STD_INSIST(!colorFromCieLab(101.0, 0.0, 0.0, color));
        STD_INSIST(!colorFromTekHvc(0.0, 50.0, -1.0, color));
    }

    STD_TEST(GamutMapsOutOfRangeChromaticity) {
        Color color{};

        STD_INSIST(colorFromCieXyz(2.0, 0.5, -1.0, color));
        STD_INSIST(!(color == Color{0, 0, 0}));
    }
}
