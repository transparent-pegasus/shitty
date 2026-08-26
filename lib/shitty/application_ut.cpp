/*
 * Copyright (C) 2026 Shitty team
 * MIT licensed
 * See the file LICENSE.MIT for the full license.
 */

#include "session.h"
#include "composer.h"
#include "application.h"

#include <std/tst/ut.h>
#include <std/str/view.h>
#include <std/mem/obj_pool.h>

#include <signal.h>
#include <plt/input.h>
#include <plt/poller.h>
#include <plt/window.h>
#include <plt/platform.h>
#include <plt/poller_loop.h>
#include <plt/platform_headless.h>

using namespace stl;

namespace {
    struct SavedSignals {
        SavedSignals() {
            STD_INSIST(sigaction(SIGCHLD, nullptr, &child) == 0);
            STD_INSIST(sigaction(SIGINT, nullptr, &interrupt) == 0);
            STD_INSIST(sigaction(SIGQUIT, nullptr, &quit) == 0);
        }

        ~SavedSignals() noexcept {
            sigaction(SIGCHLD, &child, nullptr);
            sigaction(SIGINT, &interrupt, nullptr);
            sigaction(SIGQUIT, &quit, nullptr);
        }

        struct sigaction child{};
        struct sigaction interrupt{};
        struct sigaction quit{};
    };

    struct DriveApplication final: plt::TimerCallback {
        explicit DriveApplication(Composer& composer_)
            : composer(composer_)
        {
        }

        void ready() override {
            fired = true;
            auto& window = static_cast<plt::WindowHeadless&>(*composer.window);
            framePresented = window.dispatchFrame();

            // Enter one line through the same platform-facing sink used by
            // Wayland/Cocoa. This crosses FiberInputSink, InputRouter,
            // SessionSet and Vterm before reaching the real PTY handle.
            composer.input->text({.codepoint = 'g'});
            composer.input->text({.codepoint = 'o'});
            composer.input->key({
                .key = plt::InputKey::Enter,
                .action = plt::InputAction::Press,
            });
            composer.input->flush();
        }

        Composer& composer;
        bool fired = false;
        bool framePresented = false;
    };

    struct StopOnTimeout final: plt::TimerCallback {
        explicit StopOnTimeout(plt::Platform& platform_)
            : platform(platform_)
        {
        }

        void ready() override {
            fired = true;
            platform.stop();
        }

        plt::Platform& platform;
        bool fired = false;
    };
}

STD_TEST_SUITE(ApplicationProduction) {
    STD_TEST(HeadlessRunWiresPresentsAndTearsDownProductionComponents) {
        SavedSignals savedSignals;
        // Application and its threaded Pty have the same process lifetime
        // here as in runMain. Sessions still tear down through their arenas.
        ObjPool* const pool = ObjPool::fromMemoryRaw();
        Composer& composer = *pool->make<Composer>(pool);
        plt::InputSink* const router = composer.input;
        plt::Platform* const platform = plt::createHeadlessPlatform(*pool);
        composer.platform = platform;
        auto* const poller = static_cast<plt::PollerLoop*>(platform->poller());
        DriveApplication drive(composer);
        StopOnTimeout timeout(*platform);
        poller->timeout(1, drive);
        poller->timeout(5'000'000, timeout);

        Application* const application = Application::create(composer);
        char program[] = "application_ut";
        char config[] = "-config";
        char configPath[] = "/dev/null";
        char geometry[] = "-geometry";
        char geometryValue[] = "20x4";
        char execute[] = "-e";
        char shell[] = "/bin/sh";
        char commandFlag[] = "-c";
        char script[] = "IFS= read -r line; printf '\\033]2;orchestrated\\007seen:%s\\n' \"$line\"";
        char* argv[] = {
            program,
            config,
            configPath,
            geometry,
            geometryValue,
            execute,
            shell,
            commandFlag,
            script,
            nullptr,
        };

        const int result = application->run(9, argv);
        poller->cancel(timeout);

        STD_INSIST(result == 0);
        STD_INSIST(drive.fired);
        STD_INSIST(drive.framePresented);
        STD_INSIST(!timeout.fired);
        STD_INSIST(composer.platform == platform);
        STD_INSIST(composer.input != router);
        STD_INSIST(composer.window != nullptr);
        STD_INSIST(composer.pty != nullptr);
        STD_INSIST(composer.sessions != nullptr);
        STD_INSIST(composer.renderer == nullptr);
        STD_INSIST(SessionSet::liveSessions == 0);

        auto& window = static_cast<plt::WindowHeadless&>(*composer.window);
        STD_INSIST(window.presentedFrame().generation == 1);
        STD_INSIST(window.title() == StringView(u8"orchestrated"));
    }
}
