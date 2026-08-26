# Copyright (C) 2026 Shitty team
# MIT licensed
# See the file LICENSE.MIT for the full license.

"""Merges static archives into one with an ar MRI script, so `build a`
ships a single .a carrying the facade, the VT core, libstd and the
headless libplt."""

import os
import shutil
import subprocess
import sys


def main():
    output = sys.argv[1]
    inputs = sys.argv[2:]
    script = [f"create {output}"]
    script += [f"addlib {archive}" for archive in inputs]
    script += ["save", "end", ""]
    ar = os.environ.get("AR") or shutil.which("ar") or shutil.which("llvm-ar") or "ar"
    subprocess.run([ar, "-M"], input="\n".join(script), text=True, check=True)


if __name__ == "__main__":
    main()
