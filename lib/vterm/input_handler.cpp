/*
 * Copyright (C) 2026 Shitty team
 * MIT licensed
 * See the file LICENSE.MIT for the full license.
 */

#include "input_handler.h"

InputHandler::~InputHandler() noexcept {
    unlink();
}
