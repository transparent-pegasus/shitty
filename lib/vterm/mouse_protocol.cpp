/*
 * Copyright (C) 2026 Shitty team
 * MIT licensed
 * See the file LICENSE.MIT for the full license.
 */

#include "mouse_protocol.h"

#include <lib/vterm/utf8.h>

#include <std/str/view.h>
#include <std/alg/minmax.h>
#include <std/str/builder.h>

using namespace stl;

bool encodeMouseProtocol(StringBuilder& output, MouseTrackingEnc encoding, MouseEventType type, unsigned modifiers, int motionButton, int button, int column, int row) {
    int code = 0;
    if (type == MouseEventType::Motion) {
        switch (motionButton) {
            case 1:
                code = 32;
                break;
            case 2:
                code = 33;
                break;
            case 3:
                code = 34;
                break;
            default:
                code = 35;
                break;
        }
    } else if (type == MouseEventType::Release && encoding != MouseTrackingEnc::SGR && encoding != MouseTrackingEnc::SGRPixels) {
        code = 3;
    } else {
        switch (button) {
            case 1:
                code = 0;
                break;
            case 2:
                code = 1;
                break;
            case 3:
                code = 2;
                break;
            case 4:
                code = 64;
                break;
            case 5:
                code = 65;
                break;
            case 6:
                code = 66;
                break;
            case 7:
                code = 67;
                break;
            case 8:
                code = 128;
                break;
            case 9:
                code = 129;
                break;
            case 10:
                code = 130;
                break;
            case 11:
                code = 131;
                break;
            default:
                return false;
        }
    }

    if (modifiers & MouseShift) {
        code += 4;
    }
    if (modifiers & MouseAlt) {
        code += 8;
    }
    if (modifiers & MouseControl) {
        code += 16;
    }

    switch (encoding) {
        case MouseTrackingEnc::Default:
            column = min(max(column, 1), 223);
            row = min(max(row, 1), 223);
            output << StringView(u8"\x1b[M");
            {
                const u8 bytes[] = {(u8)(32 + code), (u8)(32 + column), (u8)(32 + row)};
                output.append(bytes, sizeof(bytes));
            }
            break;
        case MouseTrackingEnc::UTF8:
            column = min(max(column, 1), 2015);
            row = min(max(row, 1), 2015);
            output << StringView(u8"\x1b[M");
            Utf8Encoder::pushUnicode(32 + code, [&output](char ch) {
                output.append(&ch, 1);
            });
            Utf8Encoder::pushUnicode(32 + column, [&output](char ch) {
                output.append(&ch, 1);
            });
            Utf8Encoder::pushUnicode(32 + row, [&output](char ch) {
                output.append(&ch, 1);
            });
            break;
        case MouseTrackingEnc::SGR:
        case MouseTrackingEnc::SGRPixels:
            output << StringView(u8"\x1b[<") << code << StringView(u8";") << column << StringView(u8";") << row << (type == MouseEventType::Release ? StringView(u8"m") : StringView(u8"M"));
            break;
        case MouseTrackingEnc::URXVT:
            output << StringView(u8"\x1b[") << code + 32 << StringView(u8";") << column << StringView(u8";") << row << StringView(u8"M");
            break;
    }
    return true;
}
