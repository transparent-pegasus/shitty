/*
 * Copyright (C) 2026 Shitty team
 * MIT licensed
 * See the file LICENSE.MIT for the full license.
 */

#pragma once

#include <std/str/view.h>
#include <std/sys/throw.h>
#include <std/str/builder.h>

// The application's throwable error: a message assembled at the raise
// site, caught as stl::Exception by the top-level handlers. This is what
// std::runtime_error used to be here.
struct FatalError final: public stl::Exception {
    explicit FatalError(stl::Buffer&& text) noexcept;
    ~FatalError() noexcept override;

    stl::ExceptionKind kind() const noexcept override;
    stl::StringView description() override;

    stl::Buffer text;
};

[[noreturn]] void raiseFatal(stl::Buffer&& text);

// raiseError(u8"-border: expected unsigned, max. ", limit) - every part
// streams through StringBuilder, so anything Outable works.
template <typename... Part>
[[noreturn]] void raiseError(const Part&... part) {
    stl::StringBuilder text;
    (text << ... << part);
    raiseFatal(static_cast<stl::Buffer&&>(text));
}
