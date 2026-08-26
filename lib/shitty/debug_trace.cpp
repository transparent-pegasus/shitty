/*
 * Copyright (C) 2026 Shitty team
 * MIT licensed
 * See the file LICENSE.MIT for the full license.
 */

#include "debug_trace.h"

#include "brand.h"
#include "options.h"
#include "composer.h"

#include <lib/vterm/hex.h>
#include <lib/vterm/vterm.h>
#include <lib/vterm/screen.h>

#include <std/sys/crt.h>
#include <std/str/view.h>
#include <std/str/builder.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

using namespace stl;

namespace {
    u64 traceStartUs = 0;

    static void writeAll(int fd, StringView text) {
        const u8* data = text.data();
        size_t left = text.length();
        while (left != 0) {
            const ssize_t written = ::write(fd, data, left);
            if (written <= 0) {
                return;
            }
            data += written;
            left -= (size_t)(written);
        }
    }
}

void openDebugTrace(Composer& composer) {
    if (composer.opts->debugTrace.empty() || composer.debugFd >= 0) {
        return;
    }
    const int fd = ::open((const char*)(composer.opts->debugTrace.data()), O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd < 0) {
        fprintf(stderr, "%s: -debug open: %s\n", composer.brand->identifierCString(), strerror(errno));
        return;
    }
    composer.debugFd = fd;
    traceStartUs = monotonicNowUs();
    debugTraceLine(composer, StringView(u8"trace opened"));
}

void debugTraceLine(Composer& composer, StringView line) {
    if (composer.debugFd < 0) {
        return;
    }
    const u64 elapsed = monotonicNowUs() - traceStartUs;
    const u64 milliseconds = (elapsed / 1000) % 1000;
    StringBuilder text;
    text << StringView(u8"[") << (i64)(elapsed / 1'000'000) << StringView(u8".");
    text << (i64)(milliseconds / 100) << (i64)((milliseconds / 10) % 10) << (i64)(milliseconds % 10);
    text << StringView(u8"] ") << line << StringView(u8"\n");
    // One write per line: the report arrives however the run ended.
    writeAll(composer.debugFd, StringView(text));
}

void debugTraceTerminal(Composer& composer, StringView what, Vterm& terminal) {
    if (composer.debugFd < 0) {
        return;
    }
    StringBuilder line;
    line << what;
    const TerminalUpdate* const update = terminal.output();
    if (update != nullptr && update->shapes != nullptr) {
        const ScreenInfo info = update->shapes->info();
        u16 sum = 0;
        if (info.rows != 0 && info.columns != 0) {
            sum = update->shapes->checksum(0, 0, (u16)(info.rows - 1), (u16)(info.columns - 1), ChecksumPositive | ChecksumNoAttributes);
        }
        line << StringView(u8" grid ") << (i64)(info.columns) << StringView(u8"x") << (i64)(info.rows);
        line << StringView(u8" history=") << (i64)(info.historyRows) << StringView(u8" view=") << (i64)(info.viewOffset);
        line << StringView(u8" sum=") << Hex{sum, 4};
    } else {
        line << StringView(u8" no pending update");
    }
    debugTraceLine(composer, StringView(line));
}
