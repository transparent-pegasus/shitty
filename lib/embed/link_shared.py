# Copyright (C) 2026 Shitty team
# MIT licensed
# See the file LICENSE.MIT for the full license.

"""Links libshitty_vt.so: the facade objects wholesale, libstd and the
headless libplt on demand, and a version script that leaves only the
shitty_vt_ entry points visible."""

import os
import subprocess
import sys


def main():
    output = sys.argv[1]
    version_script = sys.argv[2]
    whole = sys.argv[3]
    rest = sys.argv[4:]
    cxx = os.environ.get("CXX", "c++")
    subprocess.run(
        [
            cxx,
            "-shared",
            "-o",
            output,
            "-Wl,--whole-archive",
            whole,
            "-Wl,--no-whole-archive",
            *rest,
            f"-Wl,--version-script={version_script}",
            "-Wl,--no-undefined",
            "-lpthread",
            "-lm",
        ],
        check=True,
    )


if __name__ == "__main__":
    main()
