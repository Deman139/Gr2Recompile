#!/usr/bin/env python3
"""Names the host frames of a GOW3_PROFILE report (src/profiler_win.c).

Usage: symbolize_profile.py REPORT [EXE] > named.txt
Frames written as "gow3-probe.exe+0xOFFSET" get "function (file:line)" from llvm-symbolizer
(the executable carries DWARF from -g); other modules and guest offsets stay as they are.
"""
import os
import re
import shutil
import struct
import subprocess
import sys

FRAME = re.compile(r"^(\s+)(gow3-probe\.exe)\+0x([0-9a-fA-F]+)\s*$")


def image_base(path):
    with open(path, "rb") as f:
        data = f.read(4096)
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    return struct.unpack_from("<Q", data, pe + 24 + 24)[0]  # PE32+ OptionalHeader.ImageBase


def main():
    report = sys.argv[1]
    exe = sys.argv[2] if len(sys.argv) > 2 else os.path.join(os.path.dirname(__file__), "..", "out", "gow3-probe.exe")
    symbolizer = shutil.which("llvm-symbolizer") or r"C:\msys64\clang64\bin\llvm-symbolizer.exe"
    base = image_base(exe)
    with open(report, encoding="utf-8", errors="replace") as f:
        lines = f.read().splitlines()
    # Frames after the first of a stack are return addresses: name the call before them.
    keys, first = [], True
    for line in lines:
        m = FRAME.match(line)
        if m:
            keys.append((line, int(m.group(3), 16) - (0 if first else 1)))
        if line.startswith("        "):
            first = False
        else:
            first = True  # a sample share or "last:" starts a new stack
    offsets = sorted({o for _, o in keys})
    names = {}
    if offsets:
        query = "".join("0x%x\n" % (base + o) for o in offsets)
        out = subprocess.run([symbolizer, "--obj=" + exe, "--functions=linkage", "--demangle", "--no-inlines",
                              "--output-style=GNU"],
                             input=query, capture_output=True, text=True).stdout.splitlines()
        # GNU style: two lines per address (function, file:line).
        for i, o in enumerate(offsets):
            func = out[2 * i] if 2 * i < len(out) else "?"
            loc = out[2 * i + 1] if 2 * i + 1 < len(out) else "?"
            names[o] = "%s (%s)" % (func, os.path.basename(loc.replace("\\", "/")))
    first = True
    for line in lines:
        m = FRAME.match(line)
        if m:
            o = int(m.group(3), 16) - (0 if first else 1)
            print("%s%s" % (m.group(1), names.get(o, line.strip())))
        else:
            print(line)
        first = not line.startswith("        ")


if __name__ == "__main__":
    main()
