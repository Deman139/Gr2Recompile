#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Offline import check: which imports left to the host runtime (out/link.json) the C runtime
cannot resolve, without building or running the loader.

It mirrors runtime_resolve() in src/runtime.c:
  1. exact "NID#library" keys written in the sources (strcmp chains, data imports);
  2. RuntimeExport tables, matched by symbol name through src/import_names.inc
     (the NID#library key must be listed there, or runtime_symbol() returns NULL);
  3. the GPU library's LIB_FUNCTION registrations (matched by NID only).
libScePosix imports share libkernel's keys. Libraries named in GOW3_NATIVE_LIBS are never
resolved by the host when the linked module exports them (runtime_native_lib()).

usage: check_imports.py out/link.json [--aerolib path/to/shadPS4/src/core/aerolib/aerolib.inl]
Prints the missing imports grouped by library; exit status 1 when any are missing.
"""
import argparse
import collections
import json
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parent.parent


def canonical(name):
    nid, _, lib = name.partition('#')
    return f'{nid}#libkernel' if lib == 'libScePosix' else name


def host_tables():
    exact, export_names, gpu_nids = set(), set(), set()
    for f in (ROOT / 'src').glob('*.[ch]'):
        if f.name == 'import_names.inc':
            continue
        text = f.read_text(errors='replace')
        exact.update(re.findall(r'"([A-Za-z0-9+\-]{11}#lib[A-Za-z0-9]+)"', text))
        if f.name.startswith('runtime'):
            export_names.update(re.findall(r'\{"([A-Za-z_][A-Za-z0-9_]*)",\s*\(?\s*(?:void\s*\*\s*\)\s*)?[A-Za-z_]', text))
    for d in ('gpu/shadps4', 'gpu/shim'):
        for f in (ROOT / d).rglob('*.cpp'):
            gpu_nids.update(re.findall(r'LIB_FUNCTION\("([A-Za-z0-9+\-]{11})"', f.read_text(errors='replace')))
    listed = {}
    for nid, symbol in re.findall(r'\{"([^"]+)","([^"]+)"\}', (ROOT / 'src/import_names.inc').read_text()):
        listed[nid] = symbol
    return exact, export_names, gpu_nids, listed


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('link_json', type=Path)
    p.add_argument('--aerolib', type=Path, help="shadPS4's aerolib.inl, for names of unlisted NIDs")
    p.add_argument('--tolerate', nargs='*', default=[],
                   help='libraries whose unresolved imports are expected (listed, not counted)')
    a = p.parse_args()
    link = json.loads(a.link_json.read_text())
    exact, export_names, gpu_nids, listed = host_tables()
    names = {}
    if a.aerolib and a.aerolib.is_file():
        for nid, symbol in re.findall(r'STUB\("([^"]{11})",\s*([^)]+)\)', a.aerolib.read_text(errors='replace')):
            names[nid] = symbol
    for key, symbol in listed.items():
        names.setdefault(key.partition('#')[0], symbol)

    missing = collections.defaultdict(list)
    unlisted = []
    for name in link['unresolved_imports']:
        key = canonical(name)
        nid = key.partition('#')[0]
        symbol = listed.get(key) or names.get(nid, '?')
        if key in exact or nid in gpu_nids:
            continue
        if key in listed and listed[key] in export_names:
            continue
        if symbol in export_names:
            unlisted.append((key, symbol))  # implemented; its NID#library is not in import_names.inc
            continue
        missing[name.partition('#')[2]].append((nid, symbol))
    if unlisted:
        print(f'Implemented but not in src/import_names.inc ({len(unlisted)}):')
        for key, symbol in sorted(unlisted):
            print(f'    {{"{key}","{symbol}"}},')
    tolerated = sum(len(missing.pop(lib, [])) for lib in a.tolerate)
    if tolerated:
        print(f'Tolerated (left as traps, only fatal if used): {tolerated} in {", ".join(a.tolerate)}')
    for lib in sorted(missing, key=lambda k: -len(missing[k])):
        print(f'{lib} ({len(missing[lib])})')
        for nid, symbol in sorted(missing[lib], key=lambda x: x[1]):
            print(f'    {nid}  {symbol}')
    total = sum(len(v) for v in missing.values())
    print(f'Imports left to the host: {len(link["unresolved_imports"])}; missing: {total}; '
          f'unlisted: {len(unlisted)}')
    return 1 if total or unlisted else 0


if __name__ == '__main__':
    sys.exit(main())
