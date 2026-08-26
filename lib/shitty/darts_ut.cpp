/*
 * Copyright (C) 2026 Shitty team
 * MIT licensed
 * See the file LICENSE.MIT for the full license.
 */

#include "darts.h"

#include <std/tst/ut.h>
#include <std/mem/obj_pool.h>

using namespace stl;

STD_TEST_SUITE(Darts) {
    STD_TEST(ExactMatch) {
        static const StringView keys[] = {
            StringView(u8"tab"),
            StringView(u8"tabsize"),
            StringView(u8"fg"),
        };

        auto pool = ObjPool::fromMemory();
        const Darts* trie = Darts::create(*pool, keys, 3);
        STD_INSIST(trie->find(StringView(u8"tab")) == 0);
        STD_INSIST(trie->find(StringView(u8"tabsize")) == 1);
        STD_INSIST(trie->find(StringView(u8"fg")) == 2);
        STD_INSIST(trie->find(StringView(u8"ta")) == Darts::missing);
        STD_INSIST(trie->find(StringView(u8"tabsizes")) == Darts::missing);
        STD_INSIST(trie->find(StringView(u8"x")) == Darts::missing);
        STD_INSIST(trie->find(StringView()) == Darts::missing);
    }

    STD_TEST(PrefixResolution) {
        static const StringView keys[] = {
            StringView(u8"tab"),
            StringView(u8"tabsize"),
            StringView(u8"fg"),
            StringView(u8"verbose"),
            StringView(u8"version"),
            StringView(u8"v"),
        };

        auto pool = ObjPool::fromMemory();
        const Darts* trie = Darts::create(*pool, keys, 6);
        STD_INSIST(trie->resolve(StringView(u8"tab")) == 0);
        STD_INSIST(trie->resolve(StringView(u8"tabs")) == 1);
        STD_INSIST(trie->resolve(StringView(u8"t")) == Darts::ambiguous);
        STD_INSIST(trie->resolve(StringView(u8"fg")) == 2);
        STD_INSIST(trie->resolve(StringView(u8"v")) == 5);
        STD_INSIST(trie->resolve(StringView(u8"ve")) == Darts::ambiguous);
        STD_INSIST(trie->resolve(StringView(u8"verb")) == 3);
        STD_INSIST(trie->resolve(StringView(u8"vers")) == 4);
        STD_INSIST(trie->resolve(StringView(u8"x")) == Darts::missing);
        STD_INSIST(trie->resolve(StringView(u8"tabsizes")) == Darts::missing);
        STD_INSIST(trie->resolve(StringView()) == Darts::ambiguous);
    }

    STD_TEST(EmptyTrie) {
        auto pool = ObjPool::fromMemory();
        const Darts* trie = Darts::create(*pool, nullptr, 0);
        STD_INSIST(trie->find(StringView()) == Darts::missing);
        STD_INSIST(trie->find(StringView(u8"a")) == Darts::missing);
        STD_INSIST(trie->resolve(StringView(u8"a")) == Darts::missing);
    }

    STD_TEST(EmptyKey) {
        static const StringView keys[] = {
            StringView(),
            StringView(u8"a"),
        };

        auto pool = ObjPool::fromMemory();
        const Darts* trie = Darts::create(*pool, keys, 2);
        STD_INSIST(trie->find(StringView()) == 0);
        STD_INSIST(trie->find(StringView(u8"a")) == 1);
        STD_INSIST(trie->resolve(StringView()) == 0);
        STD_INSIST(trie->resolve(StringView(u8"a")) == 1);
    }
}
