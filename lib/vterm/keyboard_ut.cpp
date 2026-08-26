/*
 * Copyright (C) 2026 Shitty team
 * MIT licensed
 * See the file LICENSE.MIT for the full license.
 */

#include "keyboard.h"

#include <std/tst/ut.h>

using namespace stl;

STD_TEST_SUITE(Keyboard) {
    STD_TEST(MapsAlphabeticControlCharacters) {
        u8 character = 0;

        for (int key = 'A'; key <= 'Z'; ++key) {
            STD_INSIST(controlCharacter(key, false, character));
            STD_INSIST(character == key - 'A' + 1);
        }
    }

    STD_TEST(MapsPunctuationControlAliases) {
        u8 character = 0;

        const struct {
            int key;
            bool shifted;
            u8 expected;
        } cases[] = {
            {' ', false, 0},
            {'2', false, 0},
            {'3', false, 27},
            {'[', false, 27},
            {'4', false, 28},
            {'\\', false, 28},
            {'5', false, 29},
            {']', false, 29},
            {'6', false, 30},
            {'7', false, 31},
            {'8', false, 127},
            {'-', false, '-'},
            {'-', true, 31},
            {'/', false, 31},
            {'/', true, 127},
        };

        for (const auto& item : cases) {
            STD_INSIST(controlCharacter(item.key, item.shifted, character));
            STD_INSIST(character == item.expected);
        }
    }

    STD_TEST(PassesOtherAsciiAndRejectsNonAscii) {
        u8 character = 0;

        STD_INSIST(controlCharacter('a', false, character));
        STD_INSIST(character == 'a');
        STD_INSIST(!controlCharacter(0, false, character));
        STD_INSIST(!controlCharacter(128, false, character));
    }
}
