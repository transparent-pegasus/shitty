/*
 * Copyright (C) 2026 Shitty team
 * MIT licensed
 * See the file LICENSE.MIT for the full license.
 */

#include "composer.h"
#include "test_input.h"

#include <lib/vterm/listener.h>
#include <lib/vterm/input_handler.h>

#include <std/tst/ut.h>
#include <std/mem/obj_pool.h>

#include <plt/input.h>

using namespace stl;
using namespace plt;

namespace {
    static constexpr int testKeyUnknown = -1;
    static constexpr int testKeyA = 65;
    static constexpr int testKeyUp = 265;
    static constexpr int testKeyRightAlt = 346;
    static constexpr int testPress = 1;
    static constexpr int testRepeat = 2;
    static constexpr int testModControl = 0x0002;
    static constexpr int testModAlt = 0x0004;
    static constexpr int testModCapsLock = 0x0010;
    static constexpr int testModNumLock = 0x0020;
    static constexpr int testModAltGraph = 0x0040;

    struct CaptureInput final: public InputHandler {
        bool key(const KeyInput& input) override;
        bool text(const TextInput& input) override;
        bool pointerMotion(const PointerMotionInput&) override;
        bool pointerButton(const PointerButtonInput&) override;
        bool scroll(const ScrollInput&) override;
        void focus(bool) override;
        void pointerPresence(bool) override;
        void flush() override;

        KeyInput lastKey;
        TextInput lastText;
        size_t keys = 0;
        size_t texts = 0;
    };

    struct CountListener final: public Listener {
        void onListen(void*) override;

        size_t calls = 0;
    };
}

bool CaptureInput::key(const KeyInput& input) {
    lastKey = input;
    ++keys;
    return true;
}

bool CaptureInput::text(const TextInput& input) {
    lastText = input;
    ++texts;
    return true;
}

bool CaptureInput::pointerMotion(const PointerMotionInput&) {
    return false;
}

bool CaptureInput::pointerButton(const PointerButtonInput&) {
    return false;
}

bool CaptureInput::scroll(const ScrollInput&) {
    return false;
}

void CaptureInput::focus(bool) {
}

void CaptureInput::pointerPresence(bool) {
}

void CaptureInput::flush() {
}

void CountListener::onListen(void*) {
    ++calls;
}

STD_TEST_SUITE(TestInput) {
    STD_TEST(TranslatesPrintableAndTextInput) {
        auto pool = ObjPool::fromMemory();
        Composer& composer = *pool->make<Composer>(pool.mutPtr());
        TestInput& input = *TestInput::create(composer);
        CaptureInput capture;
        composer.inputHandlers.pushBack(&capture);

        input.key(testKeyA, 0, testPress, testModControl | testModCapsLock);
        input.text('a', testModAlt | testModNumLock);

        STD_INSIST(capture.keys == 1);
        STD_INSIST(capture.lastKey.key == InputKey::Printable);
        STD_INSIST(capture.lastKey.action == InputAction::Press);
        STD_INSIST(capture.lastKey.baseCodepoint == 'a');
        STD_INSIST(capture.lastKey.layoutCodepoint == 'a');
        STD_INSIST(capture.lastKey.modifiers == (InputControl | InputCapsLock));
        STD_INSIST(capture.texts == 1);
        STD_INSIST(capture.lastText.codepoint == 'a');
        STD_INSIST(capture.lastText.modifiers == (InputAlt | InputNumLock));
    }

    STD_TEST(TranslatesSpecialKeysAndAltGraph) {
        auto pool = ObjPool::fromMemory();
        Composer& composer = *pool->make<Composer>(pool.mutPtr());
        TestInput& input = *TestInput::create(composer);
        CaptureInput capture;
        composer.inputHandlers.pushBack(&capture);

        input.key(testKeyUp, 0, testRepeat, 0);

        STD_INSIST(capture.lastKey.key == InputKey::Up);
        STD_INSIST(capture.lastKey.action == InputAction::Repeat);
        STD_INSIST(capture.lastKey.baseCodepoint == 0);
        STD_INSIST(capture.lastKey.layoutCodepoint == 0);

        input.key(testKeyRightAlt, 0, testPress, testModAlt);

        STD_INSIST(capture.lastKey.key == InputKey::RightAlt);
        STD_INSIST((capture.lastKey.modifiers & InputAlt) != 0);
        STD_INSIST((capture.lastKey.modifiers & InputAltGraph) == 0);

        input.key(testKeyRightAlt, 0, testPress, testModAltGraph);

        STD_INSIST(capture.lastKey.key == InputKey::RightAlt);
        STD_INSIST((capture.lastKey.modifiers & InputAlt) == 0);
        STD_INSIST((capture.lastKey.modifiers & InputAltGraph) != 0);
    }

    STD_TEST(RejectsUnknownKeyEvents) {
        auto pool = ObjPool::fromMemory();
        Composer& composer = *pool->make<Composer>(pool.mutPtr());
        TestInput& input = *TestInput::create(composer);
        CaptureInput capture;
        composer.inputHandlers.pushBack(&capture);

        input.key(testKeyUnknown, 0, testPress, 0);
        input.key(testKeyA, 0, 99, 0);
        input.text(0, 0);

        STD_INSIST(capture.keys == 0);
        STD_INSIST(capture.texts == 0);
    }

    STD_TEST(PublishesContentScale) {
        auto pool = ObjPool::fromMemory();
        Composer& composer = *pool->make<Composer>(pool.mutPtr());
        TestInput& input = *TestInput::create(composer);
        CountListener listener;
        composer.contentScaleChangedListeners.pushBack(&listener);

        input.contentScale(1.25f, 1.5f);
        input.contentScale(1.5f, 1.25f);

        STD_INSIST(composer.contentScale == 1.5f);
        STD_INSIST(listener.calls == 1);
    }
}
