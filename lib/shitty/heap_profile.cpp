/*
 * Copyright (C) 2026 Shitty team
 * MIT licensed
 * See the file LICENSE.MIT for the full license.
 */

#include "heap_profile.h"

#include <lib/vterm/fatal.h>
#include <lib/vterm/num.h>

#include <std/ios/out_fd.h>
#include <std/str/builder.h>
#include <std/str/view.h>
#include <std/sys/fd.h>
#include <std/sys/throw.h>

#include <gperftools/heap-profiler.h>
#include <gperftools/malloc_extension.h>

#include <fcntl.h>
#include <stdlib.h>

#include <string>

using namespace stl;

namespace {
    static constexpr const char* defaultProfilePath = "heap.prof";
    static constexpr const char* profilePathEnvironment = "SHITTY_HEAP_PROFILE";
    static constexpr const char* sampleEnvironment = "TCMALLOC_SAMPLE_PARAMETER";
    static constexpr size_t defaultSampleInterval = 4096;
}

void initializeHeapProfile() {
    if (IsHeapProfilerRunning() != 0) {
        raiseError(StringView(u8"HEAPPROFILE cannot be combined with sampled heap profiling"));
    }
    size_t sampleInterval = defaultSampleInterval;
    if (const char* configured = getenv(sampleEnvironment); configured != nullptr) {
        u64 parsed = 0;
        if (!parseU64(StringView(configured), parsed) || parsed == 0) {
            raiseError(StringView(u8"TCMALLOC_SAMPLE_PARAMETER must be a positive integer"));
        }
        sampleInterval = parsed;
    }
    if (!MallocExtension::instance()->SetNumericProperty("tcmalloc.sample_parameter", sampleInterval)) {
        raiseError(StringView(u8"TCMalloc heap sampling is unavailable"));
    }
}

void dumpHeapProfile() {
    const char* path = getenv(profilePathEnvironment);
    if (path == nullptr || *path == 0) {
        path = defaultProfilePath;
    }

    // MallocExtension only offers a std::string sink; keep the bridge in
    // this one profiling-only translation unit.
    std::string profile;
    MallocExtension::instance()->GetHeapSample(&profile);

    const int rawFd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (rawFd < 0) {
        Errno().raise(StringBuilder() << StringView(u8"cannot open heap profile ") << StringView(path));
    }

    ScopedFD fd(rawFd);
    FDRegular output(fd);
    output.writeC(profile.data(), profile.size());
}
