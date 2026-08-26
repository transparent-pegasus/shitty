/*
 * Copyright (C) 2026 Shitty team
 * MIT licensed
 * See the file LICENSE.MIT for the full license.
 */

#include "input_remap.h"

#include "brand.h"
#include "options.h"
#include "composer.h"
#include "input_keys.h"

#include <lib/vterm/listener.h>

#include <std/ios/sys.h>
#include <std/str/view.h>
#include <std/sym/i_map.h>
#include <std/sym/s_map.h>
#include <std/mem/obj_pool.h>

#include <string.h>
#include <plt/input.h>

using namespace stl;

namespace {
    // Lock state must not defeat a chord match.
    constexpr u16 ignoredModifiers = plt::InputCapsLock | plt::InputNumLock;

    // A chord names either a key from the generated table or one printable
    // ASCII character, matched against the ASCII-layout identity of the
    // event so a remap keeps working on any active layout.
    struct RemapTarget {
        bool drop;
        plt::InputKey key;
        u32 codepoint;
        u16 modifiers;
    };

    static u64 chordKey(u16 modifiers, u64 identity) {
        return ((u64)(modifiers) << 32) | identity;
    }

    static u64 namedIdentity(plt::InputKey key) {
        return 0x200000u + (u64)(key);
    }

    static u8 foldAscii(u8 byte) {
        if (byte >= 'A' && byte <= 'Z') {
            return byte - 'A' + 'a';
        }
        return byte;
    }

    struct InputRemapState {
        InputRemapState(ObjPool& owner, Composer& composer);

        bool apply(plt::KeyInput& input);

    private:
        bool parseRule(StringView rule);
        bool parseChord(StringView text, u16& modifiers, u64& identity, RemapTarget& target, bool source);
        u64 eventIdentity(const plt::KeyInput& input) const;
        bool rewrite(plt::KeyInput& input, const RemapTarget& target) const;

        SymbolMap<plt::InputKey> nameToKey_;
        IntMap<RemapTarget> chords_;
        IntMap<RemapTarget> pressed_;
    };

    struct InputRemapImpl final: public InputRemap, public Listener {
        explicit InputRemapImpl(Composer& composer);
        ~InputRemapImpl() noexcept;

        bool apply(plt::KeyInput& input) override;
        void onListen(void*) override;

        Composer& composer_;
        ObjPool* statePool_ = nullptr;
        InputRemapState* state_ = nullptr;
    };
}

InputRemapState::InputRemapState(ObjPool& owner, Composer& composer)
    : nameToKey_(&owner)
    , chords_(&owner)
    , pressed_(&owner)
{
    for (const InputKeyName& entry : inputKeyNames) {
        nameToKey_.insert(StringView(entry.name), entry.key);
    }
    for (size_t index = 0; index < composer.opts->remaps.length(); ++index) {
        const StringView rule = composer.opts->remaps[index];
        if (!parseRule(rule)) {
            const StringView identifier = composer.brand->identifier();
            sysE << identifier << StringView(u8": remap: ignoring unparsable rule '") << rule << StringView(u8"'") << endL;
        }
    }
}

bool InputRemapState::parseChord(StringView text, u16& modifiers, u64& identity, RemapTarget& target, bool source) {
    modifiers = 0;
    identity = 0;
    target = {};
    if (!source && text == StringView("none")) {
        target.drop = true;
        return true;
    }
    size_t begin = 0;
    while (begin < text.length()) {
        size_t end = begin;
        while (end < text.length() && text[end] != '+') {
            ++end;
        }
        const StringView token(text.data() + begin, end - begin);
        const bool last = end == text.length();
        begin = end + 1;
        if (!last) {
            if (token == StringView("ctrl") || token == StringView("control")) {
                modifiers |= plt::InputControl;
            } else if (token == StringView("shift")) {
                modifiers |= plt::InputShift;
            } else if (token == StringView("alt") || token == StringView("opt") || token == StringView("option")) {
                modifiers |= plt::InputAlt;
            } else if (token == StringView("super") || token == StringView("cmd") || token == StringView("command") || token == StringView("meta")) {
                modifiers |= plt::InputSuper;
            } else {
                return false;
            }
            continue;
        }
        if (token.length() == 1 && token[0] > ' ' && token[0] < 0x7f) {
            const u8 folded = foldAscii(token[0]);
            identity = folded;
            target.key = plt::InputKey::Printable;
            target.codepoint = folded;
            target.modifiers = modifiers;
            return true;
        }
        const plt::InputKey* const named = nameToKey_.find(token);
        if (named == nullptr || *named == plt::InputKey::Unknown || *named == plt::InputKey::Printable) {
            return false;
        }
        identity = namedIdentity(*named);
        target.key = *named;
        target.codepoint = 0;
        target.modifiers = modifiers;
        return true;
    }
    return false;
}

bool InputRemapState::parseRule(StringView rule) {
    size_t split = 0;
    while (split < rule.length() && rule[split] != '=') {
        ++split;
    }
    if (split == 0 || split == rule.length()) {
        return false;
    }
    const StringView from(rule.data(), split);
    const StringView into(rule.data() + split + 1, rule.length() - split - 1);
    u16 fromModifiers = 0;
    u64 fromIdentity = 0;
    RemapTarget fromTarget = {};
    if (!parseChord(from, fromModifiers, fromIdentity, fromTarget, true)) {
        return false;
    }
    u16 intoModifiers = 0;
    u64 intoIdentity = 0;
    RemapTarget intoTarget = {};
    if (!parseChord(into, intoModifiers, intoIdentity, intoTarget, false)) {
        return false;
    }
    chords_.insert(chordKey(fromModifiers, fromIdentity), intoTarget);
    return true;
}

u64 InputRemapState::eventIdentity(const plt::KeyInput& input) const {
    if (input.key != plt::InputKey::Printable && input.key != plt::InputKey::Unknown) {
        return namedIdentity(input.key);
    }
    const u32 base = input.baseCodepoint != 0 ? input.baseCodepoint : input.layoutCodepoint;
    if (base == 0 || base > 0x7e) {
        return 0;
    }
    return foldAscii((u8)(base));
}

bool InputRemapState::rewrite(plt::KeyInput& input, const RemapTarget& target) const {
    if (target.drop) {
        return false;
    }
    input.key = target.key;
    input.modifiers = target.modifiers | (input.modifiers & ignoredModifiers);
    input.layoutCodepoint = target.codepoint;
    input.shiftedCodepoint = 0;
    input.baseCodepoint = target.codepoint;
    return true;
}

bool InputRemapState::apply(plt::KeyInput& input) {
    const u64 identity = eventIdentity(input);
    if (identity == 0) {
        return true;
    }
    if (input.action == plt::InputAction::Press) {
        // A fresh press decides; repeat and release of the same physical
        // key then follow it even when the modifiers were released first,
        // so a remapped press never leaks an unremapped release.
        pressed_.erase(identity);
        const RemapTarget* const target = chords_.find(chordKey(input.modifiers & ~ignoredModifiers, identity));
        if (target == nullptr) {
            return true;
        }
        pressed_.insert(identity, *target);
        return rewrite(input, *target);
    }
    const RemapTarget* const held = pressed_.find(identity);
    if (held == nullptr) {
        return true;
    }
    const RemapTarget target = *held;
    if (input.action == plt::InputAction::Release) {
        pressed_.erase(identity);
    }
    return rewrite(input, target);
}

InputRemapImpl::InputRemapImpl(Composer& composer)
    : composer_(composer)
    , statePool_(ObjPool::fromMemoryRaw())
{
    try {
        state_ = statePool_->make<InputRemapState>(*statePool_, composer_);
    } catch (...) {
        delete statePool_;
        statePool_ = nullptr;
        throw;
    }
    composer_.configChangedListeners.pushBack(this);
}

InputRemapImpl::~InputRemapImpl() noexcept {
    delete statePool_;
}

bool InputRemapImpl::apply(plt::KeyInput& input) {
    return state_->apply(input);
}

void InputRemapImpl::onListen(void*) {
    ObjPool* const nextPool = ObjPool::fromMemoryRaw();
    InputRemapState* next;
    try {
        next = nextPool->make<InputRemapState>(*nextPool, composer_);
    } catch (...) {
        delete nextPool;
        return;
    }
    ObjPool* const previousPool = statePool_;
    statePool_ = nextPool;
    state_ = next;
    delete previousPool;
}

InputRemap* InputRemap::create(Composer& composer) {
    return composer.pool->make<InputRemapImpl>(composer);
}
