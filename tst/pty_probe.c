/*
 * Copyright (C) 2026 Shitty team
 * MIT licensed
 * See the file LICENSE.MIT for the full license.
 */

/* Characterizes the host kernel's pty behavior, no fibers or schedulers
 * in the loop - the ground truth behind the two Darwin-gated pty unit
 * tests. Run it on a Mac and paste the output:
 *
 *   ./build pty_probe && ./pty_probe
 *
 * Probe 1: does TIOCSWINSZ on the master deliver SIGWINCH to the child?
 * Probe 2: how much does the master accept while nobody reads the
 *          slave, and does a blocking write ever push back? */

#if defined(__APPLE__)
    #define _DARWIN_C_SOURCE
#else
    #define _XOPEN_SOURCE 600
    #define _DEFAULT_SOURCE
#endif

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <unistd.h>

#include <stdlib.h>

#if defined(__APPLE__)
    /* The production spawn path goes through forkpty; on Darwin its
     * header is <util.h>, which nothing shadows. */
    #include <util.h>
#endif

/* The by-hand spawn: posix_openpt end to end. On Linux this stands in
 * for forkpty too, whose <pty.h> is shadowed by the project's own
 * header on the build's include path. */
static int spawn_pty(int* master_out) {
    const int master = posix_openpt(O_RDWR | O_NOCTTY);
    if (master < 0 || grantpt(master) != 0 || unlockpt(master) != 0) {
        return -1;
    }
    const char* const name = ptsname(master);
    if (name == NULL) {
        close(master);
        return -1;
    }
    const pid_t child = fork();
    if (child < 0) {
        close(master);
        return -1;
    }
    if (child == 0) {
        setsid();
        const int slave = open(name, O_RDWR);
        if (slave < 0) {
            _exit(1);
        }
        if (ioctl(slave, TIOCSCTTY, 0) != 0) {
            dprintf(slave, "note: TIOCSCTTY failed: %s\n", strerror(errno));
        }
        dup2(slave, 0);
        dup2(slave, 1);
        dup2(slave, 2);
        if (slave > 2) {
            close(slave);
        }
        return 0;
    }
    *master_out = master;
    return (int)(child);
}

static int read_line(int fd, char* out, size_t cap, int timeout_ms) {
    size_t used = 0;
    while (used + 1 < cap) {
        struct pollfd event = {fd, POLLIN, 0};
        const int ready = poll(&event, 1, timeout_ms);
        if (ready <= 0) {
            return -1;
        }
        char byte;
        const ssize_t got = read(fd, &byte, 1);
        if (got <= 0) {
            return -1;
        }
        if (byte == '\n') {
            break;
        }
        if (byte == '\r') {
            continue;
        }
        out[used++] = byte;
    }
    out[used] = 0;
    return 0;
}

static int spawn_production(int* master_out) {
#if defined(__APPLE__)
    int master = -1;
    const pid_t child = forkpty(&master, NULL, NULL, NULL);
    if (child < 0) {
        return -1;
    }
    if (child == 0) {
        return 0;
    }
    *master_out = master;
    return (int)(child);
#else
    /* forkpty's header is shadowed here; the by-hand path is the
     * established Linux baseline anyway. */
    return spawn_pty(master_out);
#endif
}

enum WinchWait {
    WaitSigwait,
    WaitHandler,
    WaitPollSize,
};

static volatile sig_atomic_t handlerFired = 0;

static void winchHandler(int signal) {
    (void)(signal);
    handlerFired = 1;
}

static void probe_winch(const char* how, int (*spawn)(int*), enum WinchWait wait) {
    int master = -1;
    const pid_t child = spawn(&master);
    if (child < 0) {
        perror("spawn");
        return;
    }
    if (child == 0) {
        sigset_t signals;
        sigemptyset(&signals);
        sigaddset(&signals, SIGTTOU);
        sigprocmask(SIG_BLOCK, &signals, NULL);
        /* What a shell does first: claim the foreground. */
        if (tcsetpgrp(STDIN_FILENO, getpgrp()) != 0) {
            printf("note: tcsetpgrp failed: %s\n", strerror(errno));
        }
        struct winsize size = {0, 0, 0, 0};
        ioctl(STDIN_FILENO, TIOCGWINSZ, &size);
        if (wait == WaitSigwait) {
            sigemptyset(&signals);
            sigaddset(&signals, SIGWINCH);
            sigprocmask(SIG_BLOCK, &signals, NULL);
        } else if (wait == WaitHandler) {
            struct sigaction action;
            memset(&action, 0, sizeof(action));
            action.sa_handler = winchHandler;
            sigaction(SIGWINCH, &action, NULL);
        }
        printf("ready\n");
        fflush(stdout);
        if (wait == WaitSigwait) {
            int received = 0;
            sigwait(&signals, &received);
        } else if (wait == WaitHandler) {
            while (!handlerFired) {
                pause();
            }
        } else {
            /* No signals at all: watch the size itself, which answers
             * whether the master-side ioctl reaches the slave tty. */
            struct winsize next = size;
            for (int slept = 0; slept < 300; ++slept) {
                ioctl(STDIN_FILENO, TIOCGWINSZ, &next);
                if (next.ws_row != size.ws_row || next.ws_col != size.ws_col) {
                    break;
                }
                usleep(10000);
            }
        }
        ioctl(STDIN_FILENO, TIOCGWINSZ, &size);
        printf("winch %u %u\n", (unsigned)(size.ws_row), (unsigned)(size.ws_col));
        fflush(stdout);
        _exit(0);
    }
    char line[128];
    for (;;) {
        if (read_line(master, line, sizeof(line), 3000) != 0) {
            printf("probe 1 (%s): no ready handshake\n", how);
            kill(child, SIGKILL);
            waitpid(child, NULL, 0);
            close(master);
            return;
        }
        if (strstr(line, "note:") != NULL) {
            printf("probe 1 (%s): %s\n", how, line);
            continue;
        }
        if (strstr(line, "ready") != NULL) {
            break;
        }
    }
    struct winsize size = {47, 123, 984, 752};
    if (ioctl(master, TIOCSWINSZ, &size) != 0) {
        perror("TIOCSWINSZ");
    }
    if (read_line(master, line, sizeof(line), 4000) == 0) {
        printf("probe 1 (%s): woke up, child saw '%s'\n", how, line);
    } else {
        printf("probe 1 (%s): nothing within 4s\n", how);
    }
    kill(child, SIGKILL);
    waitpid(child, NULL, 0);
    close(master);
}

static void probe_pushback(const char* how, int (*spawn)(int*)) {
    int master = -1;
    const pid_t child = spawn(&master);
    if (child < 0) {
        perror("spawn");
        return;
    }
    if (child == 0) {
        /* The slave is open once this line arrives; then nothing reads,
         * ever. */
        printf("ready\n");
        fflush(stdout);
        for (;;) {
            pause();
        }
    }
    char line[128];
    for (;;) {
        if (read_line(master, line, sizeof(line), 3000) != 0) {
            printf("probe 2 (%s): no ready handshake\n", how);
            kill(child, SIGKILL);
            waitpid(child, NULL, 0);
            close(master);
            return;
        }
        if (strstr(line, "ready") != NULL) {
            break;
        }
    }
    /* Terminal processing off, or the tty layer's echo and signal
     * handling muddy the accounting. */
    struct termios raw;
    if (tcgetattr(master, &raw) == 0) {
        cfmakeraw(&raw);
        tcsetattr(master, TCSANOW, &raw);
    }
    const int flags = fcntl(master, F_GETFL);
    fcntl(master, F_SETFL, flags | O_NONBLOCK);
    static char chunk[65536];
    memset(chunk, 'x', sizeof(chunk));
    unsigned long long accepted = 0;
    for (;;) {
        const ssize_t wrote = write(master, chunk, sizeof(chunk));
        if (wrote > 0) {
            accepted += (unsigned long long)(wrote);
            if (accepted >= 256ull * 1024 * 1024) {
                printf("probe 2 (%s): master swallowed 256MB without EAGAIN - bytes are being dropped\n", how);
                break;
            }
            continue;
        }
        if (wrote < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            printf("probe 2 (%s): master accepted %llu bytes, then EAGAIN (pushback works)\n", how, accepted);
            break;
        }
        printf("probe 2 (%s): write failed after %llu bytes: %s\n", how, accepted, strerror(errno));
        break;
    }
    fcntl(master, F_SETFL, flags);
    kill(child, SIGKILL);
    waitpid(child, NULL, 0);
    close(master);
}

int main(void) {
    probe_winch("production spawn, sigwait", spawn_production, WaitSigwait);
    probe_winch("production spawn, signal handler", spawn_production, WaitHandler);
    probe_winch("production spawn, poll size only", spawn_production, WaitPollSize);
    probe_pushback("production spawn", spawn_production);
    return 0;
}
