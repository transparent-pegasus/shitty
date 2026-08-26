# Copyright (C) 2026 Shitty team
# MIT licensed
# See the file LICENSE.MIT for the full license.

"""Packs the embedding facade into a release tarball: the static and
shared libraries, the header, and a pkg-config file carrying the link
flags this very build probed - the answer to "what does the archive
need on its link line" that a consumer cannot derive from the archive
itself (issue 102).

The .pc resolves its paths through ${pcfiledir}, so the unpacked tree
works from anywhere:

    PKG_CONFIG_PATH=<unpacked>/lib/pkgconfig pkg-config --static --libs shitty_vt
"""

import os
import shutil
import subprocess
import sys
import tempfile


def main():
    split = sys.argv.index("--")
    output, version, header, archive, shared = sys.argv[1:split]
    private_libs = sys.argv[split + 1 :]

    root = f"shitty_vt-{version}"
    with tempfile.TemporaryDirectory() as staging:
        top = os.path.join(staging, root)
        os.makedirs(os.path.join(top, "include"))
        os.makedirs(os.path.join(top, "lib", "pkgconfig"))
        shutil.copyfile(header, os.path.join(top, "include", "shitty_vt.h"))
        shutil.copyfile(archive, os.path.join(top, "lib", "libshitty_vt.a"))
        shutil.copyfile(shared, os.path.join(top, "lib", "libshitty_vt.so"))
        with open(os.path.join(top, "lib", "pkgconfig", "shitty_vt.pc"), "w") as pc:
            pc.write(
                "prefix=${pcfiledir}/../..\n"
                "libdir=${prefix}/lib\n"
                "includedir=${prefix}/include\n"
                "\n"
                "Name: shitty_vt\n"
                "Description: The shitty VT core as an embeddable C library\n"
                f"Version: {version}\n"
                "Libs: -L${libdir} -lshitty_vt\n"
                f"Libs.private: {' '.join(private_libs)}\n"
                "Cflags: -I${includedir}\n"
            )
        subprocess.run(
            ["tar", "czf", os.path.abspath(output), root],
            cwd=staging,
            check=True,
        )


if __name__ == "__main__":
    main()
