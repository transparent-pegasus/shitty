/*
 * Copyright (C) 2026 Shitty team
 * MIT licensed
 * See the file LICENSE.MIT for the full license.
 */

#include "options.h"
#include "composer.h"
#include "configuration.h"

#include <lib/vterm/listener.h>

#include <std/sys/fd.h>
#include <std/tst/ut.h>
#include <std/sys/mem_fd.h>
#include <std/str/builder.h>
#include <std/mem/obj_pool.h>

#include <unistd.h>

using namespace stl;

namespace {
    struct TempConfig {
        TempConfig();

        void replace(StringView text);
        void rewind();
        char* path();

        ScopedFD fd;
        StringBuilder fdPath;
    };

    struct ConfigListener final: public Listener {
        explicit ConfigListener(Composer& composer);

        void onListen(void* argument) override;

        Composer& composer;
        const Options* observed = nullptr;
        u8 fontsize = 0;
        u16 border = 0;
        size_t calls = 0;
        bool argumentWasNull = false;
    };
}

TempConfig::TempConfig()
    : fd(memFD("config-ut"))
{
    // Not every kernel exposes /dev/fd (this development host lacks it);
    // /proc/self/fd is the Linux spelling of the same directory. macOS
    // has only /dev/fd, so probe rather than pin either.
    if (access("/dev/fd", F_OK) == 0) {
        fdPath << StringView(u8"/dev/fd/") << fd.get();
    } else {
        fdPath << StringView(u8"/proc/self/fd/") << fd.get();
    }
}

char* TempConfig::path() {
    return fdPath.cStr();
}

void TempConfig::replace(StringView text) {
    STD_INSIST(ftruncate(fd.get(), 0) == 0);
    rewind();
    const u8* data = text.data();
    size_t left = text.length();
    while (left != 0) {
        const size_t written = fd.write(data, left);
        STD_INSIST(written != 0);
        data += written;
        left -= written;
    }
    rewind();
}

void TempConfig::rewind() {
    STD_INSIST(lseek(fd.get(), 0, SEEK_SET) == 0);
}

ConfigListener::ConfigListener(Composer& composer_)
    : composer(composer_)
{
}

void ConfigListener::onListen(void* argument) {
    observed = composer.opts;
    fontsize = composer.opts->fontsize;
    border = composer.opts->border;
    ++calls;
    argumentWasNull = argument == nullptr;
}

STD_TEST_SUITE(Config) {
    STD_TEST(PublishesEverySuccessfulReloadAsANewSnapshot) {
        auto pool = ObjPool::fromMemory();
        Composer& composer = *pool->make<Composer>(pool.mutPtr());
        Config& config = *Config::create(composer);
        TempConfig file;
        file.replace(StringView(u8"fontsize = 17\nborder = 3\n"));
        char program[] = "st";
        char configOption[] = "-config";
        char* arguments[] = {program, configOption, file.path(), nullptr};
        int count = 3;

        config.initialize(&count, arguments);
        const Options* const first = composer.opts;
        ConfigListener listener(composer);
        composer.configChangedListeners.pushBack(&listener);

        file.replace(StringView(u8"fontsize = 23\nborder = 9\n"));
        config.reload();

        STD_INSIST(composer.opts != first);
        STD_INSIST(composer.opts->fontsize == 23);
        STD_INSIST(composer.opts->border == 9);
        STD_INSIST(listener.observed == composer.opts);
        STD_INSIST(listener.fontsize == 23);
        STD_INSIST(listener.border == 9);
        STD_INSIST(listener.calls == 1);
        STD_INSIST(listener.argumentWasNull);

        const Options* const second = composer.opts;
        file.rewind();
        config.reload();

        STD_INSIST(composer.opts != second);
        STD_INSIST(listener.calls == 2);
    }

    STD_TEST(CommandLineOverridesAreReappliedOnReload) {
        auto pool = ObjPool::fromMemory();
        Composer& composer = *pool->make<Composer>(pool.mutPtr());
        Config& config = *Config::create(composer);
        TempConfig file;
        file.replace(StringView(u8"fontsize = 17\n"));
        char program[] = "st";
        char configOption[] = "-config";
        char fontsizeOption[] = "-fontsize";
        char fontsize[] = "31";
        char* arguments[] = {program, configOption, file.path(), fontsizeOption, fontsize, nullptr};
        int count = 5;

        config.initialize(&count, arguments);
        file.replace(StringView(u8"fontsize = 23\n"));
        config.reload();

        STD_INSIST(composer.opts->fontsize == 31);
    }

    STD_TEST(RejectsBrokenReloadWithoutPublishingIt) {
        auto pool = ObjPool::fromMemory();
        Composer& composer = *pool->make<Composer>(pool.mutPtr());
        Config& config = *Config::create(composer);
        TempConfig file;
        file.replace(StringView(u8"fontsize = 17\n"));
        char program[] = "st";
        char configOption[] = "-config";
        char* arguments[] = {program, configOption, file.path(), nullptr};
        int count = 3;

        config.initialize(&count, arguments);
        const Options* const original = composer.opts;
        ConfigListener listener(composer);
        composer.configChangedListeners.pushBack(&listener);
        file.replace(StringView(u8"fontsize = ???\n"));

        config.reload();

        STD_INSIST(composer.opts == original);
        STD_INSIST(composer.opts->fontsize == 17);
        STD_INSIST(listener.calls == 0);
    }
}
