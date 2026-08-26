/*
 * Copyright (C) 2026 Shitty team
 * MIT licensed
 * See the file LICENSE.MIT for the full license.
 */

#pragma once

#include <std/sys/types.h>

#include <stddef.h>

namespace stl {
    class ObjPool;
    class Output;
}

namespace plt {
    struct Platform;
    struct Window;
}

struct VtCellExtras;
struct VtConfig;
struct VtGeometry;
struct VtHost;
struct Vterm;
struct VtermTraceFactory;

struct VtermHeadless {
    virtual void feed(const u8* data, size_t len) = 0;
    // The one terminal this host built and feeds; the host owns it for
    // the process lifetime, there is no session set to ask.
    virtual Vterm* terminal() = 0;
    // The headless platform and window the host runs on, for embedders
    // that share its loop or its chrome - a test driving a real pty or
    // a session set next to the terminal.
    virtual plt::Platform* platform() = 0;
    virtual plt::Window* window() = 0;
    // The embedding pieces the host built around its terminal, for a
    // test that grows a second terminal against the same window.
    virtual VtHost* host() = 0;
    virtual VtGeometry& geometry() = 0;
    virtual VtCellExtras& extras() = 0;

    // The host builds every embedding piece itself - geometry, config
    // slot, extras, allocator, platform - around the caller's config.
    // ptyCapture observes what the terminal writes toward its child;
    // null discards it.
    static VtermHeadless* create(stl::ObjPool& pool, const VtConfig& config, VtermTraceFactory* traceFactory, stl::Output* ptyCapture = nullptr);
};
