/*
 * Copyright (C) 2026 Shitty team
 * MIT licensed
 * See the file LICENSE.MIT for the full license.
 */

#pragma once

#include <std/lib/node.h>

#include <plt/input.h>

struct InputHandler: stl::IntrusiveNode {
    virtual bool key(const plt::KeyInput& input) = 0;
    virtual bool text(const plt::TextInput& input) = 0;
    virtual bool pointerMotion(const plt::PointerMotionInput& input) = 0;
    virtual bool pointerButton(const plt::PointerButtonInput& input) = 0;
    virtual bool scroll(const plt::ScrollInput& input) = 0;
    virtual void focus(bool focused) = 0;
    virtual void pointerPresence(bool present) = 0;
    virtual void flush() = 0;

    ~InputHandler() noexcept;
};
