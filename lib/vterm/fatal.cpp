/*
 * Copyright (C) 2026 Shitty team
 * MIT licensed
 * See the file LICENSE.MIT for the full license.
 */

#include "fatal.h"

using namespace stl;

FatalError::FatalError(Buffer&& text_) noexcept
    : text(static_cast<Buffer&&>(text_)) {
}

FatalError::~FatalError() noexcept {
}

ExceptionKind FatalError::kind() const noexcept {
    return ExceptionKind::Verify;
}

StringView FatalError::description() {
    return StringView(text);
}

void raiseFatal(Buffer&& text) {
    throw FatalError(static_cast<Buffer&&>(text));
}
