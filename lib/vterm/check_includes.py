# Copyright (C) 2026 Shitty team
# MIT licensed
# See the file LICENSE.MIT for the full license.

"""The lib/vterm boundary: the VT core includes only its own headers,
plt, libstd and system headers. Any mention of lib/shitty - by full
path or by a quoted include that only resolves there - is a violation.
The whole point of the split is that the core cannot see the GUI, the
fonts or the renderers; this check is the guarantee."""

import re
import sys
from pathlib import Path


# Headers generated into the build directory; a quoted include cannot
# resolve inside lib/vterm, and they are ours, not lib/shitty's.
GENERATED = {
    "parser.rl.h",
    "utf8_dfa.h",
    "input_keys.h",
    "unicode_data.h",
}

INCLUDE = re.compile(r'^\s*#\s*include\s+(<[^>]+>|"[^"]+")')


def check(root):
    sources = sorted(
        path
        for pattern in ("*.h", "*.cpp")
        for path in root.glob(pattern)
    )
    local = {path.name for path in sources}
    violations = []
    for path in sources:
        for number, line in enumerate(path.read_text().splitlines(), 1):
            match = INCLUDE.match(line)
            if match is None:
                continue
            spec = match.group(1)
            name = spec[1:-1]
            where = f"{path.name}:{number}"
            if "lib/shitty" in name:
                violations.append(f"{where}: {spec} crosses into lib/shitty")
            elif spec.startswith('"'):
                if name not in local and name not in GENERATED:
                    violations.append(
                        f"{where}: {spec} does not resolve inside lib/vterm"
                    )
            elif name.startswith("lib/") and not name.startswith("lib/vterm/"):
                violations.append(f"{where}: {spec} reaches outside the core")
    return violations


def main():
    root = Path(sys.argv[1])
    stamp = Path(sys.argv[2])
    violations = check(root)
    if violations:
        for violation in violations:
            print(violation, file=sys.stderr)
        return 1
    stamp.write_text("ok\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
