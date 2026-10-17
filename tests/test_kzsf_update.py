from paths import ROOT
import subprocess
import sys
import tempfile
import time
import unittest
import zipfile
from pathlib import Path

sys.path.insert(0, str(ROOT / 'launcher'))
import kzsf_update as u  # noqa: E402

EXE = 'Killzone Shadow Fall.exe'


def release(tag, asset=True, draft=False):
    assets = [{'name': f'KillzoneShadowFall-{tag}-windows.zip', 'browser_download_url': f'https://x/{tag}.zip',
               'size': 10}] if asset else []
    return {'tag_name': tag, 'draft': draft, 'html_url': f'https://x/{tag}', 'body': 'notes', 'assets': assets}


class UpdateTests(unittest.TestCase):
    def test_versions_compare_numerically(self):
        self.assertGreater(u.parse_version('v0.10.0'), u.parse_version('v0.9.9'))
        self.assertEqual(u.parse_version('0.2'), (0, 2, 0))
        self.assertIsNone(u.parse_version('nightly'))

    def test_newest_release_skips_drafts_and_releases_without_zip(self):
        best = u.newest_release([release('v0.1.1'), release('v0.3.0', draft=True), release('v0.2.5', asset=False),
                                 release('v0.2.0')])
        self.assertEqual(best['version'], '0.2.0')
        self.assertEqual(best['url'], 'https://x/v0.2.0.zip')

    def test_unsafe_zip_is_rejected(self):
        with tempfile.TemporaryDirectory() as d:
            bad = Path(d) / 'bad.zip'
            with zipfile.ZipFile(bad, 'w') as z:
                z.writestr('../evil.txt', 'x')
            with self.assertRaises(ValueError):
                u.extract(bad, Path(d) / 'out', EXE)

    @unittest.skipUnless(sys.platform == 'win32', 'the update script is a Windows .cmd')
    def test_update_waits_for_launcher_and_keeps_user_folder(self):
        # The restarted "launcher" may still run when the folder goes away.
        with tempfile.TemporaryDirectory(ignore_cleanup_errors=True) as d:
            port = Path(d) / 'port Ñ'
            (port / 'user').mkdir(parents=True)
            (port / 'user' / 'save.txt').write_text('save')
            (port / 'old.txt').write_text('old')
            (port / EXE).write_bytes(b'')
            archive = Path(d) / 'KillzoneShadowFall-v9.9.9-windows.zip'
            with zipfile.ZipFile(archive, 'w') as z:
                z.writestr('Killzone Shadow Fall/old.txt', 'new')
                z.write(r'C:\Windows\System32\whoami.exe', f'Killzone Shadow Fall/{EXE}')
            work = port / 'user' / 'update'
            source = u.extract(archive, work / 'new', EXE)
            launcher = subprocess.Popen(['ping', '-n', '3', '127.0.0.1'], stdout=subprocess.DEVNULL)
            script = u.install_script(source, port, launcher.pid, EXE, work, cleanup=(work / 'new', archive))
            start = time.time()
            u.run_script(script).wait(timeout=60)
            launcher.wait()
            self.assertGreater(time.time() - start, 1.5)  # waited for the launcher to exit
            self.assertEqual((port / 'old.txt').read_text(), 'new')
            self.assertEqual((port / 'user' / 'save.txt').read_text(), 'save')
            self.assertFalse((work / 'new').exists() or archive.exists())
            self.assertFalse(u.failed_update(work))


if __name__ == '__main__':
    unittest.main()
