/*
 * Copyright (C) 2026 Shitty team
 * MIT licensed
 * See the file LICENSE.MIT for the full license.
 */

#include "listener.h"

void Listener::onListen() {
    onListen(nullptr);
}

Listener::~Listener() noexcept {
    unlink();
}
