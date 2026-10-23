#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Adds the imports a game leaves to the host runtime (out/link.json) to src/import_names.inc,
named from shadPS4's aerolib.inl: runtime_lookup() finds RuntimeExport functions by these names.
Existing lines are kept; new ones are appended in sorted order. libScePosix keys are written as
libkernel, as runtime.c canonicalizes them.

usage: add_import_names.py out/link.json path/to/shadPS4/src/core/aerolib/aerolib.inl
"""
import json
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parent.parent
EXTRA = {'fJgP+wqifno': 'sceDiscMap_7C980FFB0AA27E7A'}


def main():
    link = json.loads(Path(sys.argv[1]).read_text())
    names = dict(re.findall(r'STUB\("([^"]{11})",\s*([^)]+)\)', Path(sys.argv[2]).read_text(errors='replace')))
    names.update(EXTRA)
    inc = ROOT / 'src' / 'import_names.inc'
    text = inc.read_text()
    have = set(re.findall(r'\{"([^"]+)",', text))
    new = []
    for name in link['unresolved_imports']:
        nid, _, lib = name.partition('#')
        key = f'{nid}#libkernel' if lib == 'libScePosix' else name
        if key in have or nid not in names:
            continue
        have.add(key)
        new.append(f'{{"{key}","{names[nid]}"}},')
    if new:
        inc.write_text(text.rstrip('\n') + '\n' + '\n'.join(sorted(new)) + '\n')
    print(f'{len(new)} names added to {inc.relative_to(ROOT)}')


if __name__ == '__main__':
    main()
