/*
 * Copyright (C) 2026 Shitty team
 * MIT licensed
 * See the file LICENSE.MIT for the full license.
 */

#include "input_router.h"

#include "composer.h"
#include <lib/vterm/input_handler.h>

#include <std/mem/obj_pool.h>
#include <std/tst/ut.h>

using namespace stl;
using namespace plt;

namespace {
    struct CaptureHandler final: public InputHandler {
        bool key(const KeyInput& input) override;
        bool text(const TextInput& input) override;
        bool pointerMotion(const PointerMotionInput& input) override;
        bool pointerButton(const PointerButtonInput& input) override;
        bool scroll(const ScrollInput& input) override;
        void focus(bool focused) override;
        void pointerPresence(bool present) override;
        void flush() override;

        bool consume = false;
        bool unlinkOnKey = false;
        KeyInput lastKey;
        TextInput lastText;
        PointerMotionInput lastMotion;
        PointerButtonInput lastButton;
        ScrollInput lastScroll;
        size_t keys = 0;
        size_t texts = 0;
        size_t motions = 0;
        size_t buttons = 0;
        size_t scrolls = 0;
        size_t focuses = 0;
        size_t presences = 0;
        size_t flushes = 0;
        bool focused = false;
        bool present = false;
    };
}

bool CaptureHandler::key(const KeyInput& input) {
    lastKey = input;
    ++keys;
    if (unlinkOnKey) {
        unlink();
    }
    return consume;
}

bool CaptureHandler::text(const TextInput& input) {
    lastText = input;
    ++texts;
    return consume;
}

bool CaptureHandler::pointerMotion(const PointerMotionInput& input) {
    lastMotion = input;
    ++motions;
    return consume;
}

bool CaptureHandler::pointerButton(const PointerButtonInput& input) {
    lastButton = input;
    ++buttons;
    return consume;
}

bool CaptureHandler::scroll(const ScrollInput& input) {
    lastScroll = input;
    ++scrolls;
    return consume;
}

void CaptureHandler::focus(bool focused_) {
    focused = focused_;
    ++focuses;
}

void CaptureHandler::pointerPresence(bool present_) {
    present = present_;
    ++presences;
}

void CaptureHandler::flush() {
    ++flushes;
}

STD_TEST_SUITE(InputRouter) {
    STD_TEST(StopsAtFirstConsumingHandler) {
        auto pool = ObjPool::fromMemory();
        Composer& composer = *pool->make<Composer>(pool.mutPtr());
        CaptureHandler first;
        CaptureHandler second;
        first.consume = true;
        composer.inputHandlers.pushBack(&first);
        composer.inputHandlers.pushBack(&second);

        composer.input->key({InputKey::Enter});
        STD_INSIST(first.keys == 1);
        STD_INSIST(second.keys == 0);
    }

    STD_TEST(ContinuesAfterHandlerRemovesItself) {
        auto pool = ObjPool::fromMemory();
        Composer& composer = *pool->make<Composer>(pool.mutPtr());
        CaptureHandler removing;
        CaptureHandler trailing;
        removing.unlinkOnKey = true;
        composer.inputHandlers.pushBack(&removing);
        composer.inputHandlers.pushBack(&trailing);

        composer.input->key({InputKey::Enter});
        STD_INSIST(removing.keys == 1);
        STD_INSIST(trailing.keys == 1);

        composer.input->key({InputKey::Enter});
        STD_INSIST(removing.keys == 1);
        STD_INSIST(trailing.keys == 2);
    }

    STD_TEST(RoutesEveryInputShape) {
        auto pool = ObjPool::fromMemory();
        Composer& composer = *pool->make<Composer>(pool.mutPtr());
        CaptureHandler sink;
        composer.inputHandlers.pushBack(&sink);

        composer.input->text({0x20ac, InputAlt});
        composer.input->pointerMotion({12, 34, InputShift});
        composer.input->pointerButton({PointerButton::Middle, true, 12, 34, InputControl, 1.5});
        composer.input->scroll({
            .x = 1.25,
            .y = -2.5,
            .pixelX = 12,
            .pixelY = 34,
            .modifiers = InputSuper,
            .phase = ScrollPhase::Update,
            .precise = true,
            .momentum = true,
            .time = 7.5,
        });

        STD_INSIST(sink.texts == 1);
        STD_INSIST(sink.lastText.codepoint == 0x20ac);
        STD_INSIST(sink.motions == 1);
        STD_INSIST(sink.lastMotion.pixelY == 34);
        STD_INSIST(sink.buttons == 1);
        STD_INSIST(sink.lastButton.button == PointerButton::Middle);
        STD_INSIST(sink.scrolls == 1);
        STD_INSIST(sink.lastScroll.y == -2.5);
        STD_INSIST(sink.lastScroll.phase == ScrollPhase::Update);
        STD_INSIST(sink.lastScroll.precise);
        STD_INSIST(sink.lastScroll.momentum);
        STD_INSIST(sink.lastScroll.time == 7.5);
    }

    STD_TEST(BroadcastsStateAndFlushEvents) {
        auto pool = ObjPool::fromMemory();
        Composer& composer = *pool->make<Composer>(pool.mutPtr());
        CaptureHandler first;
        CaptureHandler second;
        composer.inputHandlers.pushBack(&first);
        composer.inputHandlers.pushBack(&second);

        composer.input->focus(true);
        composer.input->pointerPresence(true);
        composer.input->flush();

        STD_INSIST(first.focuses == 1 && second.focuses == 1);
        STD_INSIST(first.focused && second.focused);
        STD_INSIST(first.presences == 1 && second.presences == 1);
        STD_INSIST(first.present && second.present);
        STD_INSIST(first.flushes == 1 && second.flushes == 1);
    }
}
