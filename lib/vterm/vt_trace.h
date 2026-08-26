/*
 * Copyright (C) 2026 Shitty team
 * MIT licensed
 * See the file LICENSE.MIT for the full license.
 */

#pragma once

#include <std/str/view.h>
#include <std/sys/types.h>
#include <std/lib/buffer.h>

#include <stddef.h>

struct TestApi;
struct VtermTrace;

// Handed to Vterm::create; construct() is invoked once during terminal
// construction with the terminal's test api (null outside test builds)
// and returns the trace to install, or null to disable tracing. Neither
// pointer may be used until Vterm::create returns.
struct VtermTraceFactory {
    virtual VtermTrace* construct(TestApi* testApi) = 0;
};

enum class VtermTraceString : u8 {
    Osc,
    Dcs,
    Apc,
    Pm,
    Sos
};

struct VtermTrace {
    virtual void text(const u8* data, size_t size) = 0;
    virtual void control(u8 ch) = 0;
    virtual void escapeBegin() = 0;
    virtual void escapeByte(u8 ch) = 0;
    virtual void escapeEnd() = 0;
    virtual void escapeCancel() = 0;
    virtual void csi(u8 finalByte, stl::StringView privatePrefix, stl::StringView intermediates, const u32* parameters, const unsigned char* separators, size_t parameterCount, bool hadParameters) = 0;
    virtual void stringBegin(VtermTraceString type) = 0;
    virtual void stringData(const u8* data, size_t size) = 0;
    virtual void stringEnd() = 0;
    virtual void stringCancel() = 0;
    virtual void drain(stl::Buffer& out) = 0;
    virtual void clear() = 0;

    virtual void osc(u32 command, stl::StringView payload) = 0;
    virtual void bell() = 0;
    virtual void leds(u8 state) = 0;
    virtual void cwd(stl::StringView path) = 0;
    virtual void notify(stl::StringView id, stl::StringView title, stl::StringView body, bool close) = 0;
    virtual void progress(u32 state, u32 percent) = 0;
    virtual void windowOperation(u32 operation, u32 first, u32 second) = 0;
    virtual void drainActions(stl::Buffer& out) = 0;
    virtual stl::StringView currentCwd() const = 0;
};
