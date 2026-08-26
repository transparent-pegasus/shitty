# Copyright (C) 2026 Shitty team
# MIT licensed
# See the file LICENSE.MIT for the full license.

import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


ASCII_LOWER = bytes.maketrans(
    b"ABCDEFGHIJKLMNOPQRSTUVWXYZ",
    b"abcdefghijklmnopqrstuvwxyz",
)
FORBIDDEN = b"shitty"


def main():
    if len(sys.argv) != 2:
        raise SystemExit(f"usage: {sys.argv[0]} BINARY")

    source = Path(sys.argv[1])
    with tempfile.TemporaryDirectory() as directory:
        stripped = Path(directory) / source.name
        shutil.copyfile(source, stripped)
        # The nix environment ships binutils strip, the ix one only
        # llvm-strip; both cut the symbol names this check must ignore.
        strip = shutil.which("strip") or shutil.which("llvm-strip")
        if strip is None:
            raise SystemExit("no strip tool available")
        subprocess.run([strip, stripped], check=True)
        binary = stripped.read_bytes().translate(ASCII_LOWER)

    offset = binary.find(FORBIDDEN)
    if offset >= 0:
        context = binary[max(0, offset - 32) : offset + 40]
        raise SystemExit(
            f"{source}: forbidden branding at byte offset {offset}: {context!r}"
        )


if __name__ == "__main__":
    main()
