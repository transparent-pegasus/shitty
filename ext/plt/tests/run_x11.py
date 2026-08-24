"""Run one integration executable against an isolated Xvfb server."""

import os
import select
import shutil
import signal
import subprocess
import sys
import tempfile


def fail(message, log=None):
    print(message, file=sys.stderr)
    if log is not None:
        log.seek(0)
        output = log.read()
        if output:
            print("--- Xvfb output ---", file=sys.stderr)
            print(output.decode(errors="replace"), file=sys.stderr, end="")
    return 1


def main():
    executable = sys.argv[1:]
    if not executable:
        return fail("usage: run_x11.py EXECUTABLE [ARG ...]")

    xvfb = shutil.which("Xvfb")
    if xvfb is None:
        if os.environ.get("PLT_X11_TEST_REQUIRED") == "1":
            return fail("Xvfb is required for the X11 integration tests but was not found")
        print("X11 integration tests: SKIP (Xvfb not found)", file=sys.stderr)
        return 0

    display_read, display_write = os.pipe()
    with tempfile.TemporaryFile() as log:
        server = subprocess.Popen(
            [
                xvfb,
                "-displayfd",
                str(display_write),
                "-screen",
                "0",
                "1024x768x24",
                "-nolisten",
                "tcp",
                "-noreset",
            ],
            pass_fds=(display_write,),
            stdout=log,
            stderr=subprocess.STDOUT,
            start_new_session=True,
        )
        os.close(display_write)
        try:
            readable, _, _ = select.select([display_read], [], [], 10)
            if not readable:
                return fail("Xvfb did not publish a display number", log)
            display_number = os.read(display_read, 64).decode().strip()
            if not display_number or server.poll() is not None:
                return fail("Xvfb exited before the test started", log)

            environment = os.environ.copy()
            environment["DISPLAY"] = f":{display_number}"
            environment.pop("WAYLAND_DISPLAY", None)
            environment.pop("WAYLAND_SOCKET", None)
            test = subprocess.Popen(
                executable,
                env=environment,
                start_new_session=True,
            )
            try:
                return test.wait(100)
            except subprocess.TimeoutExpired:
                os.killpg(test.pid, signal.SIGKILL)
                test.wait()
                return fail(f"X11 integration tests timed out: {' '.join(executable)}", log)
        finally:
            os.close(display_read)
            if server.poll() is None:
                os.killpg(server.pid, signal.SIGTERM)
                try:
                    server.wait(5)
                except subprocess.TimeoutExpired:
                    os.killpg(server.pid, signal.SIGKILL)
                    server.wait()


if __name__ == "__main__":
    sys.exit(main())
