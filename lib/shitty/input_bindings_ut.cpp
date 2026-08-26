/*
 * Copyright (C) 2026 Shitty team
 * MIT licensed
 * See the file LICENSE.MIT for the full license.
 */

#include "options.h"
#include "composer.h"
#include "input_bindings.h"

#include <lib/vterm/listener.h>

#include <std/tst/ut.h>
#include <std/mem/obj_pool.h>

using namespace stl;
using namespace plt;

namespace {
    struct CountBinding final: public Listener {
        void onListen(void*) override;

        size_t calls = 0;
    };

#if defined(__APPLE__)
    static constexpr u16 copyModifiers = InputSuper;
    static constexpr u16 inactiveCopyModifiers = InputControl | InputShift;
    static constexpr u16 incFontModifiers = InputSuper;
    static constexpr u32 incFontText = 0;
    static constexpr u16 tabModifiers = InputSuper;
    static constexpr u16 tabSwitchModifiers = InputSuper | InputShift;
#elif defined(__linux__)
    static constexpr u16 copyModifiers = InputControl | InputShift;
    static constexpr u16 inactiveCopyModifiers = InputSuper;
    static constexpr u16 incFontModifiers = InputControl | InputShift;
    static constexpr u32 incFontText = '+';
    static constexpr u16 tabModifiers = InputControl | InputShift;
    static constexpr u16 tabSwitchModifiers = InputControl | InputShift;
#else
    #error Unsupported platform
#endif
}

void CountBinding::onListen(void*) {
    ++calls;
}

STD_TEST_SUITE(InputBindings) {
    STD_TEST(MatchesNormalizedModifiersAndPublishes) {
        auto pool = ObjPool::fromMemory();
        Composer& composer = *pool->make<Composer>(pool.mutPtr());
        CountBinding listener;
        composer.copyListeners.pushBack(&listener);

        const bool consumed = composer.inputBindings->key({
            .key = InputKey::Printable,
            .action = InputAction::Press,
            .modifiers = copyModifiers | InputCapsLock | InputNumLock,
            .baseCodepoint = 'c',
        });

        STD_INSIST(consumed);
        STD_INSIST(listener.calls == 1);
        STD_INSIST(!composer.inputBindings->text({'c', copyModifiers}));
        STD_INSIST(composer.inputBindings->key({InputKey::Printable, InputAction::Release, copyModifiers, 0, 'c'}));
    }

    STD_TEST(NamedKeyChordIgnoresTheBaseCodepoint) {
        auto pool = ObjPool::fromMemory();
        Composer& composer = *pool->make<Composer>(pool.mutPtr());
        CountBinding listener;
        composer.pageUpListeners.pushBack(&listener);

        // Cocoa reports named keys with the function-key private-use
        // codepoint in the base field (0xf72c for PageUp); the chord
        // identity is the key itself.
        STD_INSIST(composer.inputBindings->key({InputKey::PageUp, InputAction::Press, InputShift, 0, 0xf72c}));
        STD_INSIST(listener.calls == 1);
        STD_INSIST(composer.inputBindings->key({InputKey::PageUp, InputAction::Release, InputShift, 0, 0xf72c}));
    }

    STD_TEST(DoesNotConsumeMismatchedBinding) {
        auto pool = ObjPool::fromMemory();
        Composer& composer = *pool->make<Composer>(pool.mutPtr());
        CountBinding listener;
        composer.copyListeners.pushBack(&listener);

        STD_INSIST(!composer.inputBindings->key({InputKey::Printable, InputAction::Press, inactiveCopyModifiers, 0, 'c'}));
        STD_INSIST(!composer.inputBindings->key({InputKey::Printable, InputAction::Press, copyModifiers, 0, 'b'}));
        STD_INSIST(listener.calls == 0);
    }

    STD_TEST(TracksRepeatedPlatformBinding) {
        auto pool = ObjPool::fromMemory();
        Composer& composer = *pool->make<Composer>(pool.mutPtr());
        IntrusiveList listeners;
        CountBinding listener;
        listeners.pushBack(&listener);
        composer.inputBindings->add(InputActions::IncFontSize, &listeners);
        const KeyInput input{InputKey::Printable, InputAction::Press, incFontModifiers, '=', '='};

        STD_INSIST(composer.inputBindings->key(input));
        STD_INSIST(composer.inputBindings->key(input));
        STD_INSIST(listener.calls == 2);
        if (incFontText != 0) {
            STD_INSIST(composer.inputBindings->text({incFontText, incFontModifiers}));
            STD_INSIST(composer.inputBindings->text({incFontText, incFontModifiers}));
        }
        STD_INSIST(!composer.inputBindings->text({'+', incFontModifiers}));
    }

    STD_TEST(FlushDropsPendingTextButReleaseRemainsConsumed) {
        auto pool = ObjPool::fromMemory();
        Composer& composer = *pool->make<Composer>(pool.mutPtr());
        IntrusiveList listeners;
        CountBinding listener;
        listeners.pushBack(&listener);
        composer.inputBindings->add(InputActions::IncFontSize, &listeners);
        composer.input->key({InputKey::Printable, InputAction::Press, incFontModifiers, '=', '='});

        composer.input->flush();

        STD_INSIST(!composer.inputBindings->text({'+', incFontModifiers}));
        STD_INSIST(composer.inputBindings->key({InputKey::Printable, InputAction::Release, incFontModifiers, '=', '='}));
    }

    STD_TEST(FocusLossClearsConsumedAndPendingState) {
        auto pool = ObjPool::fromMemory();
        Composer& composer = *pool->make<Composer>(pool.mutPtr());
        IntrusiveList listeners;
        CountBinding listener;
        listeners.pushBack(&listener);
        composer.inputBindings->add(InputActions::IncFontSize, &listeners);
        composer.input->key({InputKey::Printable, InputAction::Press, incFontModifiers, '=', '='});

        composer.input->focus(false);

        STD_INSIST(!composer.inputBindings->text({'+', incFontModifiers}));
        STD_INSIST(!composer.inputBindings->key({InputKey::Printable, InputAction::Release, incFontModifiers, '=', '='}));
    }

    // The tab chords must exist in both platform blocks: add() asserts it
    // found a row for the action, so a row present on one platform and
    // missing on the other trips registration on the platform that lacks
    // it rather than failing quietly.
    STD_TEST(TabActionsAreBoundOnThisPlatform) {
        auto pool = ObjPool::fromMemory();
        Composer& composer = *pool->make<Composer>(pool.mutPtr());
        CountBinding newTab;
        CountBinding nextTab;
        composer.newTabListeners.pushBack(&newTab);
        composer.nextTabListeners.pushBack(&nextTab);

        STD_INSIST(composer.inputBindings->key({InputKey::Printable, InputAction::Press, tabModifiers, 0, 't'}));
        STD_INSIST(newTab.calls == 1);
        STD_INSIST(composer.inputBindings->key({InputKey::Printable, InputAction::Press, tabSwitchModifiers, 0, ']'}));
        STD_INSIST(nextTab.calls == 1);
    }

    // Shift is part of the switch chord, and the frontends disagree about
    // whether the base codepoint of a shifted bracket is the bracket or
    // the brace: Apple documents charactersIgnoringModifiers as keeping
    // Shift, the cocoa backend's own comment says it strips it. Matching
    // is exact on the base codepoint, so binding one form leaves the
    // chord silently dead wherever the other is reported. Bind both.
    STD_TEST(TabSwitchMatchesBracketAndBraceAlike) {
        auto pool = ObjPool::fromMemory();
        Composer& composer = *pool->make<Composer>(pool.mutPtr());
        CountBinding prev;
        CountBinding next;
        composer.prevTabListeners.pushBack(&prev);
        composer.nextTabListeners.pushBack(&next);

        STD_INSIST(composer.inputBindings->key({InputKey::Printable, InputAction::Press, tabSwitchModifiers, 0, '['}));
        STD_INSIST(composer.inputBindings->key({InputKey::Printable, InputAction::Press, tabSwitchModifiers, 0, '{'}));
        STD_INSIST(prev.calls == 2);

        STD_INSIST(composer.inputBindings->key({InputKey::Printable, InputAction::Press, tabSwitchModifiers, 0, ']'}));
        STD_INSIST(composer.inputBindings->key({InputKey::Printable, InputAction::Press, tabSwitchModifiers, 0, '}'}));
        STD_INSIST(next.calls == 2);
    }

    // Cmd+L is what a Mac user reaches for to clear the screen; the
    // Linux habit is Ctrl+L, which the shell handles itself and which the
    // terminal must keep forwarding untouched.
    STD_TEST(NaturalEditingBindsOnlyBehindThePreset) {
        auto pool = ObjPool::fromMemory();
        Composer& composer = *pool->make<Composer>(pool.mutPtr());
        CountBinding wordLeft;
        CountBinding killLine;
        composer.wordLeftListeners.pushBack(&wordLeft);
        composer.killLineListeners.pushBack(&killLine);

        // Off by default: the chords stay the application's.
        STD_INSIST(!composer.inputBindings->key({InputKey::Left, InputAction::Press, InputAlt, 0, 0}));
        STD_INSIST(wordLeft.calls == 0);

        Options preset;
        preset.naturalEditing = true;
        const Options* const previous = composer.opts;
        composer.setOptions(&preset);
        const bool wordConsumed = composer.inputBindings->key({InputKey::Left, InputAction::Press, InputAlt, 0, 0});
        const bool killConsumed = composer.inputBindings->key({InputKey::Backspace, InputAction::Press, InputSuper, 0, 0});
        composer.setOptions(previous);
#if defined(__APPLE__)
        STD_INSIST(wordConsumed);
        STD_INSIST(wordLeft.calls == 1);
        STD_INSIST(killConsumed);
        STD_INSIST(killLine.calls == 1);
#else
        // The preset table only exists on macOS; Alt+Left stays the
        // application's CSI 1;3D elsewhere.
        STD_INSIST(!wordConsumed);
        STD_INSIST(wordLeft.calls == 0);
        STD_INSIST(!killConsumed);
        STD_INSIST(killLine.calls == 0);
#endif
    }

    STD_TEST(ClearIsBoundWhereThePlatformExpectsIt) {
        auto pool = ObjPool::fromMemory();
        Composer& composer = *pool->make<Composer>(pool.mutPtr());
        CountBinding clear;
        composer.clearListeners.pushBack(&clear);

#if defined(__APPLE__)
        STD_INSIST(composer.inputBindings->key({InputKey::Printable, InputAction::Press, InputSuper, 0, 'l'}));
        STD_INSIST(clear.calls == 1);
        // Plain Ctrl+L stays the shell's business.
        STD_INSIST(!composer.inputBindings->key({InputKey::Printable, InputAction::Press, InputControl, 0, 'l'}));
        STD_INSIST(clear.calls == 1);
#else
        STD_INSIST(composer.inputBindings->key({InputKey::Printable, InputAction::Press, InputControl | InputShift, 0, 'l'}));
        STD_INSIST(clear.calls == 1);
        STD_INSIST(!composer.inputBindings->key({InputKey::Printable, InputAction::Press, InputControl, 0, 'l'}));
        STD_INSIST(clear.calls == 1);
#endif
    }

    STD_TEST(ActionCanHaveMultiplePlatformBindings) {
        auto pool = ObjPool::fromMemory();
        Composer& composer = *pool->make<Composer>(pool.mutPtr());
        CountBinding listener;
        composer.pastePrimaryListeners.pushBack(&listener);

        STD_INSIST(composer.inputBindings->key({InputKey::Insert, InputAction::Press, InputShift}));
        STD_INSIST(composer.inputBindings->key({InputKey::Insert, InputAction::Release, InputShift}));
        STD_INSIST(composer.inputBindings->key({InputKey::Keypad0, InputAction::Press, InputShift | InputCapsLock}));
        STD_INSIST(composer.inputBindings->key({InputKey::Keypad0, InputAction::Release, InputShift}));
        STD_INSIST(listener.calls == 2);
    }
}
