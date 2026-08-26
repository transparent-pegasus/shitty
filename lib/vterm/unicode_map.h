/*
 * Copyright (C) 2026 Shitty team
 * MIT licensed
 * See the file LICENSE.MIT for the full license.
 */

#pragma once

#include <std/sys/types.h>
#include <std/dbg/insist.h>
#include <std/mem/obj_pool.h>

template <typename V>
struct UnicodeMap {
    static constexpr u32 codepointCount = 0x110000;
    static constexpr u32 valuesPerPage = 0x100;
    static constexpr u32 pageCount = codepointCount / valuesPerPage;

    static UnicodeMap* create(stl::ObjPool& pool);

    UnicodeMap(const UnicodeMap&) = delete;
    UnicodeMap& operator=(const UnicodeMap&) = delete;
    explicit UnicodeMap(stl::ObjPool& pool);

    V* find(u32 codepoint) noexcept;
    const V* find(u32 codepoint) const noexcept;
    V& operator[](u32 codepoint);

    size_t allocatedPages() const noexcept;

private:
    struct Page {
        V values[valuesPerPage]{};
    };

    stl::ObjPool* pool_;
    Page* pages_[pageCount]{};
};

template <typename V>
UnicodeMap<V>* UnicodeMap<V>::create(stl::ObjPool& pool) {
    return pool.make<UnicodeMap>(pool);
}

template <typename V>
UnicodeMap<V>::UnicodeMap(stl::ObjPool& pool)
    : pool_(&pool)
{
}

template <typename V>
V* UnicodeMap<V>::find(u32 codepoint) noexcept {
    if (codepoint >= codepointCount) {
        return nullptr;
    }

    auto* page = pages_[codepoint / valuesPerPage];

    if (page == nullptr) {
        return nullptr;
    }

    return &page->values[codepoint % valuesPerPage];
}

template <typename V>
const V* UnicodeMap<V>::find(u32 codepoint) const noexcept {
    if (codepoint >= codepointCount) {
        return nullptr;
    }

    const auto* page = pages_[codepoint / valuesPerPage];

    if (page == nullptr) {
        return nullptr;
    }

    return &page->values[codepoint % valuesPerPage];
}

template <typename V>
V& UnicodeMap<V>::operator[](u32 codepoint) {
    STD_INSIST(codepoint < codepointCount);

    auto& page = pages_[codepoint / valuesPerPage];

    if (page == nullptr) {
        page = pool_->make<Page>();
    }

    return page->values[codepoint % valuesPerPage];
}

template <typename V>
size_t UnicodeMap<V>::allocatedPages() const noexcept {
    size_t result = 0;

    for (const Page* page : pages_) {
        result += page != nullptr;
    }

    return result;
}
