/*
 * Copyright (C) 2026 Shitty team
 * MIT licensed
 * See the file LICENSE.MIT for the full license.
 */

#pragma once

#include <std/lib/node.h>

struct Listener: stl::IntrusiveNode {
    virtual void onListen(void* argument) = 0;

    void onListen();

    ~Listener() noexcept;
};
