/*
 * Copyright (C) 2026 Shitty team
 * MIT licensed
 * See the file LICENSE.MIT for the full license.
 */

#if defined(__APPLE__)
    /* SIGWINCH hides behind strict POSIX on Darwin. */
    #define _DARWIN_C_SOURCE
#else
    /* cfmakeraw does the same behind strict POSIX elsewhere. */
    #define _DEFAULT_SOURCE
#endif
#define _POSIX_C_SOURCE 200809L

#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <unistd.h>

static volatile sig_atomic_t signal_seen = 0;

static void note_signal(int received) {
    (void)(received);
    signal_seen = 1;
}

/* No SA_RESTART: a signal must interrupt a blocked write. */
static int arm_signal(int which) {
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = note_signal;
    return sigaction(which, &action, NULL);
}

static int write_all(const char* data, size_t size) {
    while (size != 0) {
        const ssize_t written = write(STDOUT_FILENO, data, size);
        if (written <= 0) {
            return 1;
        }
        data += written;
        size -= (size_t)(written);
    }
    return 0;
}

static int ready(void) {
    static const char message[] = "ready\n";
    return write_all(message, sizeof(message) - 1);
}

static int wait_for_winsize(void) {
    /* A handler and pause, not sigprocmask and sigwait: on xnu a
     * blocked SIGWINCH never wakes sigwait, while an ordinary handler
     * receives it - measured with tst/pty_probe.c on real hardware. */
    if (arm_signal(SIGWINCH) != 0 || ready() != 0) {
        return 1;
    }
    while (!signal_seen) {
        pause();
    }
    struct winsize size;
    if (ioctl(STDIN_FILENO, TIOCGWINSZ, &size) != 0) {
        return 1;
    }
    char message[64];
    const int length = snprintf(message, sizeof(message), "%u %u\n", (unsigned)(size.ws_row), (unsigned)(size.ws_col));
    if (length <= 0 || (size_t)(length) >= sizeof(message)) {
        return 1;
    }
    return write_all(message, (size_t)(length));
}

static int wait_for_hangup(void) {
    sigset_t signals;
    if (signal(SIGHUP, SIG_DFL) == SIG_ERR || sigemptyset(&signals) != 0 || sigaddset(&signals, SIGHUP) != 0 || sigprocmask(SIG_UNBLOCK, &signals, NULL) != 0 || ready() != 0) {
        return 1;
    }
    for (;;) {
        pause();
    }
}

static int flood_until_hangup(void) {
    /* Same xnu sigwait trap as the winsize wait: a handler notes the
     * hangup, and without SA_RESTART it interrupts a blocked write. */
    if (arm_signal(SIGHUP) != 0) {
        return 1;
    }
    static const char payload[] = "engaged-flood\n";
    for (;;) {
        if (write_all(payload, sizeof(payload) - 1) != 0) {
            while (!signal_seen) {
                pause();
            }
            return 0;
        }
        if (signal_seen) {
            return 0;
        }
    }
}

int main(int argc, char** argv) {
    if (argc != 2) {
        return 2;
    }
    /* Raw mode, like any full-screen application: in canonical mode a
     * BSD tty discards input past its 1k line limit while the write
     * keeps succeeding, and the flood tests need the master to push
     * back instead. */
    struct termios raw;
    if (tcgetattr(STDIN_FILENO, &raw) == 0) {
        cfmakeraw(&raw);
        tcsetattr(STDIN_FILENO, TCSANOW, &raw);
    }
    if (strcmp(argv[1], "winsize") == 0) {
        return wait_for_winsize();
    }
    if (strcmp(argv[1], "hangup") == 0) {
        return wait_for_hangup();
    }
    if (strcmp(argv[1], "flood-hangup") == 0) {
        return flood_until_hangup();
    }
    return 2;
}
