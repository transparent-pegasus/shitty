/*
 * Copyright (C) 2026 Shitty team
 * MIT licensed
 * See the file LICENSE.MIT for the full license.
 */

#pragma once

#include "vterm.h"

struct VtermTestCell {
    TerminalCell cell;
    const u32* grapheme = nullptr;
    size_t graphemeSize = 0;
    CellColor underlineColor;
    u8 lineAttribute = 0;
};

struct VtermTestState {
    MouseTrackingState mouse;
    u8 kittyKeyboardFlags = 0;
    bool screenReverseVideo = false;
    u8 ledState = 0;
    bool reverseWrapMode = false;
    bool nationalReplacementMode = false;
    bool pendingWrap = false;
    TerminalCursor::Style cursorStyle = TerminalCursor::Style::hidden;
    TerminalPen pen;
    RectangleOrigin rectangleOrigin{};
    size_t hyperlinkCount = 0;
    u8 charsets[4]{};
};

struct TestApi {
    virtual VtermTestState inspect() const = 0;
    virtual bool ansiMode(u32 mode) const = 0;
    virtual bool privateMode(u32 mode) const = 0;
    virtual void hardReset() = 0;
    virtual bool tabStop(u16 column) const = 0;
    virtual void setWrapped(u16 row) = 0;
    virtual VtermTestCell cell(u16 row, u16 column) const = 0;
    virtual VtermTestCell logicalCell(i32 row, u16 column) const = 0;
    virtual u8 rowSemantic(i32 row) const = 0;
    virtual u8 semanticClick() const = 0;
    virtual bool cursorIsAtPrompt() const = 0;
    virtual void key(plt::InputKey key, VtModifier modifiers) = 0;
    virtual void character(u8 byte, VtModifier modifiers) = 0;
    virtual void kittyKey(plt::InputKey key, u16 modifiers, VtermKeyEventType event) = 0;
    virtual void kittyKey(u32 key, u32 shiftedKey, u32 baseLayoutKey, u16 modifiers, VtermKeyEventType event) = 0;
    virtual bool mouseHighlightRelease(u16 endX, u16 endY, u16 mouseX, u16 mouseY) = 0;
    virtual void locatorPosition(u16 column, u16 row, u16 pixelX, u16 pixelY, u8 buttons) = 0;
    virtual void locatorButton(u8 button, bool pressed) = 0;
    virtual void scrollUp(u16 count) = 0;
    virtual void scrollDown(u16 count) = 0;
    virtual void pageUp() = 0;
    virtual void pageDown() = 0;
    virtual void selectionStart(int pixelX, int pixelY, bool cycleSnapTo) = 0;
    virtual void selectionExtend(int pixelX, int pixelY, bool cycleSnapTo) = 0;
    virtual void selectionUpdate(int pixelX, int pixelY) = 0;
    virtual VtermTextResult selectionFinish() = 0;
    virtual bool hasSelection() const = 0;
    virtual void selectionClear() = 0;
    virtual void selectionRectangular() = 0;
    virtual bool advanceSelectionAutoscroll() = 0;
    virtual void paste(stl::StringView text) = 0;
    virtual bool pasteClipboard(bool primary) = 0;
    virtual stl::StringView hyperlinkAt(int pixelX, int pixelY) = 0;
};
