/*
 * Copyright (C) 2026 Shitty team
 * MIT licensed
 * See the file LICENSE.MIT for the full license.
 */
/* part of this file is part of Zutty.
 * Copyright (C) 2020 Tom Szilagyi
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * See the file LICENSE.GPL3 for the full license.
 */

#include "utf8.h"

size_t Utf8Decoder::decodeOne(const u8* input, size_t length, u32& codepoint) {
    codepoint = 0;
    if (length == 0) {
        return 0;
    }
    const u8 first = input[0];
    if (first < 0x80) {
        codepoint = first;
        return 1;
    }
    size_t count;
    u32 accumulator;
    if (first >= 0xc2 && first <= 0xdf) {
        count = 2;
        accumulator = first & 0x1f;
    } else if (first >= 0xe0 && first <= 0xef) {
        count = 3;
        accumulator = first & 0x0f;
    } else if (first >= 0xf0 && first <= 0xf4) {
        count = 4;
        accumulator = first & 0x07;
    } else {
        return 0;
    }
    if (length < count) {
        return 0;
    }
    for (size_t index = 1; index < count; ++index) {
        const u8 byte = input[index];
        if ((byte & 0xc0) != 0x80) {
            return 0;
        }
        accumulator = (accumulator << 6) | (byte & 0x3f);
    }
    if ((count == 3 && accumulator < 0x800) || (count == 4 && accumulator < 0x10000) || accumulator > 0x10ffff || (accumulator >= 0xd800 && accumulator <= 0xdfff)) {
        return 0;
    }
    codepoint = accumulator;
    return count;
}

bool Utf8Decoder::checkPrematureEOS() {
    if (remaining > 0) {
        remaining = 0;
        unicode = Unicode_Replacement_Character;
        return true;
    }
    return false;
}

void Utf8Decoder::reset() {
    accumulator = 0;
    unicode = 0;
    minimum = 0;
    remaining = 0;
}

bool Utf8Decoder::onUnicode(u32 ch) {
    if (!ch) {
        return false;
    }

    unicode = ch;
    return true;
}

int Utf8Decoder::pushByte(unsigned char ch) {
    if ((ch & 0xc0) == 0x80) {
        if (remaining == 0) {
            unicode = Unicode_Replacement_Character;
            return 1;
        }
        const bool invalidFirstContinuation = (remaining == 2 && accumulator == 0 && ch < 0xa0) || (remaining == 2 && accumulator == 0x0d && ch >= 0xa0) || (remaining == 3 && accumulator == 0 && ch < 0x90) || (remaining == 3 && accumulator == 4 && ch >= 0x90);
        if (invalidFirstContinuation) {
            remaining = 0;
            unicode = Unicode_Replacement_Character;
            return 2;
        }
        accumulator = (accumulator << 6) | (ch & 0x3f);
        if (--remaining != 0) {
            return 0;
        }
        if (accumulator < minimum || accumulator > 0x10ffff || (accumulator >= 0xd800 && accumulator <= 0xdfff)) {
            unicode = Unicode_Replacement_Character;
        } else {
            unicode = accumulator;
        }
        return 1;
    }

    int completed = 0;
    if (remaining > 0) {
        remaining = 0;
        unicode = Unicode_Replacement_Character;
        completed = 1;
    }
    if (ch >= 0xc2 && ch <= 0xdf) {
        accumulator = ch & 0x1f;
        remaining = 1;
        minimum = 0x80;
    } else if (ch >= 0xe0 && ch <= 0xef) {
        accumulator = ch & 0x0f;
        remaining = 2;
        minimum = 0x800;
    } else if (ch >= 0xf0 && ch <= 0xf4) {
        accumulator = ch & 0x07;
        remaining = 3;
        minimum = 0x10000;
    } else {
        unicode = Unicode_Replacement_Character;
        ++completed;
    }
    return completed;
}
