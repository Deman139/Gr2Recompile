"""Additional content (DLC) in the layout shadPS4 reads, so the port and the emulator share it.

    <game>/addcont/<TITLE_ID>/<label>/sce_sys/param.sfo        (or <game>/../addcont/...)

shadPS4's addonInstallDir is then <game>/addcont. Each folder holding a param.sfo of category "ac"
for this title is one add-on; its entitlement label is the end of the CONTENT_ID
(EP9000-CUSA04943_00-ACEUCROWXXXXXXXX -> ACEUCROWXXXXXXXX).

Unlock-key DLC packages (.pkg of a few hundred KiB: a license and a param.sfo, no data) dropped
into the game folder (or next to it, or into addcont/) are unpacked into that layout by
install_packages(); the param.sfo sits unencrypted in the package's entry table. The package is
deleted once its folder is written, as the player asked. Packages with data (the PFS image) cannot
be unpacked here and are left alone.

usage: addcont.py <game folder> [--install]   (lists the add-ons; --install unpacks packages first)
"""
import argparse
import re
import struct
import sys
from pathlib import Path

LABEL_OFFSET = 20          # UPXXXX-CUSAXXXXX_XX-<label>
LABEL = re.compile(r'^[A-Za-z0-9]{1,16}$')
PKG_LIMIT = 16 << 20       # unlock-key packages are 128 KiB; anything far larger carries data
PKG_MAGIC = b'\x7fCNT'
PARAM_SFO_ENTRY = 0x1000


def parse_sfo(data):
    magic, _version, keys, values, count = struct.unpack_from('<4sIIII', data, 0)
    if magic != b'\0PSF':
        raise ValueError('not a param.sfo')
    result = {}
    for i in range(count):
        key, fmt, size, _capacity, off = struct.unpack_from('<HHIII', data, 20 + i * 16)
        name = data[keys + key:data.index(0, keys + key)].decode()
        value = data[values + off:values + off + size]
        result[name] = (struct.unpack('<I', value[:4])[0] if fmt == 0x404 else
                        value.rstrip(b'\0').decode('utf8', 'replace'))
    return result


def label_of(values, title_id):
    """The entitlement label of an add-on's param.sfo values, or None when it is not one of this
    title's add-ons."""
    content_id = str(values.get('CONTENT_ID', ''))
    if not str(values.get('CATEGORY', '')).startswith('ac') or len(content_id) <= LABEL_OFFSET:
        return None
    if title_id and content_id[7:16] != title_id:
        return None
    label = content_id[LABEL_OFFSET:]
    return label if LABEL.match(label) else None


def roots(game):
    """The addcont folders looked at, in order: inside the game folder, then next to it."""
    game = Path(game)
    return [game / 'addcont', game.parent / 'addcont']


def install_root(game):
    """Where unpacked packages go: an existing addcont folder, else one inside the game folder."""
    return next((r for r in roots(game) if r.is_dir()), roots(game)[0])


def package_sfo(path):
    """The param.sfo bytes of an unlock-key package, or None."""
    try:
        if path.stat().st_size > PKG_LIMIT:
            return None
        data = path.read_bytes()
    except OSError:
        return None
    if data[:4] != PKG_MAGIC or len(data) < 0x40:
        return None
    count, table = struct.unpack_from('>I', data, 0x10)[0], struct.unpack_from('>I', data, 0x18)[0]
    for i in range(min(count, 4096)):
        at = table + i * 32
        if at + 24 > len(data):
            return None
        entry, _name, _flags1, _flags2, offset, size = struct.unpack_from('>IIIIII', data, at)
        if entry == PARAM_SFO_ENTRY and offset + size <= len(data):
            return data[offset:offset + size]
    return None


def install_packages(game, title_id, delete=True, log=print):
    """Unpacks the unlock-key packages of this title found in the game folder, next to it or in
    an addcont folder; returns the labels installed."""
    game = Path(game)
    places = [game, game.parent, *roots(game)]
    seen, installed = set(), []
    for place in places:
        try:
            packages = sorted(p for p in place.iterdir() if p.suffix.lower() == '.pkg' and p.is_file())
        except OSError:
            continue
        for package in packages:
            if package.resolve() in seen:
                continue
            seen.add(package.resolve())
            data = package_sfo(package)
            if data is None:
                continue
            try:
                label = label_of(parse_sfo(data), title_id)
            except (ValueError, IndexError, struct.error):
                continue
            if not label:
                continue
            target = install_root(game) / title_id / label / 'sce_sys' / 'param.sfo'
            try:
                target.parent.mkdir(parents=True, exist_ok=True)
                if not target.is_file() or target.read_bytes() != data:
                    target.write_bytes(data)
                if target.read_bytes() != data:
                    continue
                installed.append(label)
                log(f'DLC {label}: unpacked from {package.name} into {target.parents[1]}')
                if delete:
                    package.unlink()
            except OSError as error:
                log(f'DLC {label}: could not unpack {package.name}: {error}')
    return installed


def find_addons(game, title_id):
    """[(label, folder, title)] of the add-ons in the addcont folders, first one of a label wins."""
    found, labels = [], set()
    for root in roots(game):
        folder = root / title_id
        try:
            entries = sorted(p for p in folder.iterdir() if p.is_dir())
        except OSError:
            continue
        for entry in entries:
            try:
                values = parse_sfo((entry / 'sce_sys' / 'param.sfo').read_bytes()[:1 << 20])
            except (OSError, ValueError, IndexError, struct.error):
                continue
            label = label_of(values, title_id)
            if label and label not in labels:
                labels.add(label)
                found.append((label, entry, str(values.get('TITLE', label))))
    return found


def game_title_id(game):
    try:
        return str(parse_sfo((Path(game) / 'sce_sys' / 'param.sfo').read_bytes()).get('TITLE_ID', ''))
    except (OSError, ValueError, IndexError, struct.error):
        return ''


def environment_value(addons):
    """GR2_ADDONS for the runtime: label=host folder, separated by |."""
    return '|'.join(f'{label}={folder}' for label, folder, _title in addons)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('game', type=Path)
    parser.add_argument('--install', action='store_true')
    parser.add_argument('--keep', action='store_true', help='keep the packages after unpacking them')
    args = parser.parse_args()
    title = game_title_id(args.game)
    if not title:
        sys.exit(f'No sce_sys/param.sfo in {args.game}')
    if args.install:
        install_packages(args.game, title, delete=not args.keep)
    for label, folder, name in find_addons(args.game, title):
        print(f'{label}  {name}  ({folder})')
