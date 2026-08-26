/*
 * Copyright (C) 2026 Shitty team
 * MIT licensed
 * See the file LICENSE.MIT for the full license.
 */

#include "unicode_map.h"

#include <std/tst/ut.h>

using namespace stl;

namespace {
    struct Value {
        u32 first = 0;
        u32 second = 0;
    };

    struct alignas(64) AlignedValue {
        u32 value = 0;
    };

    struct TrackedValue {
        static size_t alive;

        TrackedValue();
        ~TrackedValue();
    };
}

size_t TrackedValue::alive = 0;

TrackedValue::TrackedValue() {
    ++alive;
}

TrackedValue::~TrackedValue() {
    --alive;
}

STD_TEST_SUITE(UnicodeMap) {
    STD_TEST(HasFullUnicodeRange) {
        STD_INSIST(UnicodeMap<u8>::codepointCount == 0x110000);
        STD_INSIST(UnicodeMap<u8>::valuesPerPage == 0x100);
        STD_INSIST(UnicodeMap<u8>::pageCount == 0x1100);
    }

    STD_TEST(StartsEmpty) {
        auto pool = ObjPool::fromMemory();
        auto* map = UnicodeMap<u32>::create(*pool);

        STD_INSIST(map->allocatedPages() == 0);
        STD_INSIST(map->find(0) == nullptr);
        STD_INSIST(map->find(0x10ffff) == nullptr);
    }

    STD_TEST(RejectsOutOfRangeLookup) {
        auto pool = ObjPool::fromMemory();
        auto* map = UnicodeMap<u32>::create(*pool);

        STD_INSIST(map->find(0x110000) == nullptr);
        STD_INSIST(map->find(0xffffffff) == nullptr);
        STD_INSIST(map->allocatedPages() == 0);
    }

    STD_TEST(CreatesPageOnWrite) {
        auto pool = ObjPool::fromMemory();
        auto* map = UnicodeMap<u32>::create(*pool);

        (*map)[0x42] = 17;

        STD_INSIST(map->allocatedPages() == 1);
        STD_INSIST(map->find(0x42) != nullptr);
        STD_INSIST(*map->find(0x42) == 17);
    }

    STD_TEST(DefaultInitializesPage) {
        auto pool = ObjPool::fromMemory();
        auto* map = UnicodeMap<u32>::create(*pool);

        (*map)[0x42] = 17;

        STD_INSIST(map->find(0) != nullptr);
        STD_INSIST(*map->find(0) == 0);
        STD_INSIST(*map->find(0xff) == 0);
    }

    STD_TEST(ReusesPage) {
        auto pool = ObjPool::fromMemory();
        auto* map = UnicodeMap<u32>::create(*pool);

        (*map)[0] = 1;
        (*map)[1] = 2;
        (*map)[0xfe] = 3;
        (*map)[0xff] = 4;

        STD_INSIST(map->allocatedPages() == 1);
        STD_INSIST((*map)[0] == 1);
        STD_INSIST((*map)[1] == 2);
        STD_INSIST((*map)[0xfe] == 3);
        STD_INSIST((*map)[0xff] == 4);
    }

    STD_TEST(SplitsPageBoundary) {
        auto pool = ObjPool::fromMemory();
        auto* map = UnicodeMap<u32>::create(*pool);

        (*map)[0xff] = 1;
        (*map)[0x100] = 2;

        STD_INSIST(map->allocatedPages() == 2);
        STD_INSIST(*map->find(0xff) == 1);
        STD_INSIST(*map->find(0x100) == 2);
    }

    STD_TEST(SupportsLastCodepoint) {
        auto pool = ObjPool::fromMemory();
        auto* map = UnicodeMap<u32>::create(*pool);

        (*map)[0x10ffff] = 0x12345678;

        STD_INSIST(map->allocatedPages() == 1);
        STD_INSIST(*map->find(0x10ffff) == 0x12345678);
        STD_INSIST(map->find(0x10fffe) != nullptr);
        STD_INSIST(*map->find(0x10fffe) == 0);
    }

    STD_TEST(KeepsPagesIndependent) {
        auto pool = ObjPool::fromMemory();
        auto* map = UnicodeMap<u32>::create(*pool);

        (*map)[0] = 11;
        (*map)[0x100] = 22;
        (*map)[0x10000] = 33;
        (*map)[0x10ffff] = 44;

        STD_INSIST(map->allocatedPages() == 4);
        STD_INSIST(*map->find(0) == 11);
        STD_INSIST(*map->find(0x100) == 22);
        STD_INSIST(*map->find(0x10000) == 33);
        STD_INSIST(*map->find(0x10ffff) == 44);
    }

    STD_TEST(CoversEveryUnicodePlane) {
        auto pool = ObjPool::fromMemory();
        auto* map = UnicodeMap<u32>::create(*pool);

        for (u32 plane = 0; plane < 17; ++plane) {
            (*map)[plane << 16] = plane + 1;
        }

        STD_INSIST(map->allocatedPages() == 17);

        for (u32 plane = 0; plane < 17; ++plane) {
            STD_INSIST(*map->find(plane << 16) == plane + 1);
        }
    }

    STD_TEST(ProvidesConstLookup) {
        auto pool = ObjPool::fromMemory();
        auto* map = UnicodeMap<u32>::create(*pool);

        (*map)[0x1f642] = 42;

        const UnicodeMap<u32>* constMap = map;
        const u32* value = constMap->find(0x1f642);

        STD_INSIST(value != nullptr);
        STD_INSIST(*value == 42);
        STD_INSIST(constMap->find(0x20000) == nullptr);
    }

    STD_TEST(KeepsReferencesStable) {
        auto pool = ObjPool::fromMemory();
        auto* map = UnicodeMap<u32>::create(*pool);
        u32* first = &(*map)[7];

        *first = 73;

        for (u32 page = 1; page < UnicodeMap<u32>::pageCount; ++page) {
            (*map)[page * UnicodeMap<u32>::valuesPerPage] = page;
        }

        STD_INSIST(map->allocatedPages() == UnicodeMap<u32>::pageCount);
        STD_INSIST(first == map->find(7));
        STD_INSIST(*first == 73);
    }

    STD_TEST(StoresValuesContiguouslyWithinPage) {
        auto pool = ObjPool::fromMemory();
        auto* map = UnicodeMap<u32>::create(*pool);
        u32* first = &(*map)[41];
        u32* second = &(*map)[42];

        STD_INSIST(second == first + 1);
        STD_INSIST(map->allocatedPages() == 1);
    }

    STD_TEST(StoresStructuredValues) {
        auto pool = ObjPool::fromMemory();
        auto* map = UnicodeMap<Value>::create(*pool);

        (*map)[0x1f642].first = 12;
        (*map)[0x1f642].second = 34;

        const Value* value = map->find(0x1f642);

        STD_INSIST(value != nullptr);
        STD_INSIST(value->first == 12);
        STD_INSIST(value->second == 34);
    }

    STD_TEST(StoresPointerValues) {
        auto pool = ObjPool::fromMemory();
        auto* map = UnicodeMap<void*>::create(*pool);
        u32 value = 9;

        (*map)[0x20ac] = &value;

        STD_INSIST(*map->find(0x20ac) == &value);
        STD_INSIST(*map->find(0x2000) == nullptr);
    }

    STD_TEST(PreservesOverAlignment) {
        auto pool = ObjPool::fromMemory();
        auto* map = UnicodeMap<AlignedValue>::create(*pool);
        auto* value = &(*map)[0x1f642];

        STD_INSIST((uintptr_t)value % alignof(AlignedValue) == 0);
    }

    STD_TEST(UsesPoolLifetimeForPages) {
        TrackedValue::alive = 0;

        {
            auto pool = ObjPool::fromMemory();
            auto* map = UnicodeMap<TrackedValue>::create(*pool);

            (*map)[0];
            (*map)[0x100];
            STD_INSIST(TrackedValue::alive == 2 * UnicodeMap<TrackedValue>::valuesPerPage);
        }

        STD_INSIST(TrackedValue::alive == 0);
    }

    STD_TEST(FillsEntireRange) {
        auto pool = ObjPool::fromMemory();
        auto* map = UnicodeMap<u32>::create(*pool);

        for (u32 codepoint = 0; codepoint < UnicodeMap<u32>::codepointCount; ++codepoint) {
            (*map)[codepoint] = codepoint ^ 0x5a5a5a;
        }

        STD_INSIST(map->allocatedPages() == UnicodeMap<u32>::pageCount);

        for (u32 codepoint = 0; codepoint < UnicodeMap<u32>::codepointCount; ++codepoint) {
            STD_INSIST(*map->find(codepoint) == (codepoint ^ 0x5a5a5a));
        }
    }
}
