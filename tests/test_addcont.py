from paths import ROOT  # noqa: F401  (puts scripts/ on the path)
import struct
import tempfile
import unittest
from pathlib import Path

import addcont


def sfo(values):
    keys = vals = idx = b''
    for key, value in values.items():
        data = value.encode() + b'\0'
        idx += struct.pack('<HHIII', len(keys), 0x204, len(data), len(data), len(vals))
        keys += key.encode() + b'\0'
        vals += data
    table = 20 + len(idx)
    return struct.pack('<4sIIII', b'\0PSF', 0x101, table, table + len(keys), len(values)) + idx + keys + vals


def package(content_id, category='ac'):
    """An unlock-key package: header, entry table with an unencrypted param.sfo (entry 0x1000)."""
    param = sfo({'CATEGORY': category, 'CONTENT_ID': content_id, 'TITLE': 'Gravity Rush 2 - Test',
                 'TITLE_ID': content_id[7:16]})
    header = bytearray(0x1000)
    header[:4] = b'\x7fCNT'
    struct.pack_into('>I', header, 0x10, 2)
    struct.pack_into('>I', header, 0x18, 0x800)
    header[0x40:0x40 + len(content_id)] = content_id.encode()
    struct.pack_into('>IIIIII', header, 0x800, 0x1, 0, 0x40000000, 0, 0x900, 16)
    struct.pack_into('>IIIIII', header, 0x820, 0x1000, 0, 0, 0, 0x1000, len(param))
    return bytes(header) + param


class AddcontTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.game = Path(self.temp.name) / 'Game'
        (self.game / 'sce_sys').mkdir(parents=True)
        (self.game / 'sce_sys' / 'param.sfo').write_bytes(sfo({'TITLE_ID': 'CUSA04943', 'CATEGORY': 'gd'}))

    def tearDown(self):
        self.temp.cleanup()

    def test_packages_are_unpacked_into_the_shadps4_layout_and_removed(self):
        (self.game / 'crow.pkg').write_bytes(package('EP9000-CUSA04943_00-ACEUCROWXXXXXXXX'))
        (self.game.parent / 'nier.pkg').write_bytes(package('EP9000-CUSA04943_00-ACEUX1NIERXXXXXX'))
        (self.game / 'other.pkg').write_bytes(package('EP9000-CUSA01623_00-OTHERGAMEXXXXXXX'))
        (self.game / 'theme.pkg').write_bytes(package('EP9000-CUSA04943_00-THEMEXXXXXXXXXXX', 'gd'))
        installed = addcont.install_packages(self.game, 'CUSA04943', log=lambda _m: None)
        self.assertEqual(sorted(installed), ['ACEUCROWXXXXXXXX', 'ACEUX1NIERXXXXXX'])
        self.assertFalse((self.game / 'crow.pkg').exists())
        self.assertFalse((self.game.parent / 'nier.pkg').exists())
        self.assertTrue((self.game / 'other.pkg').exists())
        self.assertTrue((self.game / 'theme.pkg').exists())
        self.assertTrue((self.game / 'addcont' / 'CUSA04943' / 'ACEUCROWXXXXXXXX' / 'sce_sys' / 'param.sfo').is_file())
        addons = addcont.find_addons(self.game, 'CUSA04943')
        self.assertEqual([a[0] for a in addons], ['ACEUCROWXXXXXXXX', 'ACEUX1NIERXXXXXX'])
        value = addcont.environment_value(addons)
        self.assertTrue(value.startswith('ACEUCROWXXXXXXXX=') and '|ACEUX1NIERXXXXXX=' in value)

    def test_an_addcont_folder_next_to_the_game_is_read(self):
        folder = self.game.parent / 'addcont' / 'CUSA04943' / 'anything' / 'sce_sys'
        folder.mkdir(parents=True)
        (folder / 'param.sfo').write_bytes(sfo({'CATEGORY': 'ac', 'CONTENT_ID': 'EP9000-CUSA04943_00-ACEUKIT17XXXXXXX'}))
        self.assertEqual([a[0] for a in addcont.find_addons(self.game, 'CUSA04943')], ['ACEUKIT17XXXXXXX'])

    def test_packages_in_the_addcont_folder_are_installed_there(self):
        root = self.game / 'addcont'
        (root / 'CUSA04943').mkdir(parents=True)
        (root / 'crow.pkg').write_bytes(package('EP9000-CUSA04943_00-ACEUCROWXXXXXXXX'))
        (root / 'CUSA04943' / 'kit.pkg').write_bytes(package('EP9000-CUSA04943_00-ACEUKIT17XXXXXXX'))
        addcont.install_packages(self.game, 'CUSA04943', log=lambda _m: None)
        self.assertEqual(sorted(p.name for p in root.rglob('*.pkg')), [])
        self.assertEqual([a[0] for a in addcont.find_addons(self.game, 'CUSA04943')],
                         ['ACEUCROWXXXXXXXX', 'ACEUKIT17XXXXXXX'])

    def test_keep_leaves_the_package(self):
        (self.game / 'crow.pkg').write_bytes(package('EP9000-CUSA04943_00-ACEUCROWXXXXXXXX'))
        addcont.install_packages(self.game, 'CUSA04943', delete=False, log=lambda _m: None)
        self.assertTrue((self.game / 'crow.pkg').exists())


if __name__ == '__main__':
    unittest.main()
