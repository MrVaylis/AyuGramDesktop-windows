import importlib.util
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


BUILD = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(BUILD))
SPEC = importlib.util.spec_from_file_location("apply_ayu_patches", BUILD / "apply_ayu_patches.py")
PATCHER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PATCHER)


class ApplyPatchTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.folder = Path(self.temp.name)
        self.repo = self.folder / "dependency"
        self.repo.mkdir()
        self.git("init", "--quiet")
        self.source = self.repo / "source.txt"
        self.source.write_text("original\n", encoding="utf-8")
        self.git("add", "source.txt")
        self.git("-c", "user.name=Test", "-c", "user.email=test@example.invalid", "commit", "--quiet", "-m", "base")
        self.base = self.git("rev-parse", "HEAD").stdout.strip()
        self.source.write_text("patched\n", encoding="utf-8")
        self.patch = self.folder / "change.patch"
        self.patch.write_text(self.git("diff").stdout, encoding="utf-8")
        self.source.write_text("original\n", encoding="utf-8")

    def git(self, *args):
        return subprocess.run(
            ["git", "-C", str(self.repo), *args],
            check=True,
            capture_output=True,
            text=True,
            encoding="utf-8",
        )

    def test_idempotent_patch_preserves_unrelated_files(self):
        unrelated = self.repo / "local.txt"
        unrelated.write_text("keep me\n", encoding="utf-8")
        self.assertEqual(PATCHER.apply_patch(self.repo, self.base, self.patch), "applied")
        self.assertEqual(PATCHER.apply_patch(self.repo, self.base, self.patch), "already applied")
        self.assertEqual(PATCHER.apply_patch(self.repo, self.base, self.patch, True), "already applied")
        self.assertEqual(self.source.read_text(encoding="utf-8"), "patched\n")
        self.assertEqual(unrelated.read_text(encoding="utf-8"), "keep me\n")

    def test_check_does_not_change_unpatched_files(self):
        with self.assertRaisesRegex(RuntimeError, "required patch is not applied"):
            PATCHER.apply_patch(self.repo, self.base, self.patch, True)
        self.assertEqual(self.source.read_text(encoding="utf-8"), "original\n")

    def test_unexpected_base_is_rejected(self):
        with self.assertRaisesRegex(RuntimeError, "expected base"):
            PATCHER.apply_patch(self.repo, "0" * 40, self.patch)
        self.assertEqual(self.source.read_text(encoding="utf-8"), "original\n")

    def test_conflict_is_rejected_without_overwriting(self):
        self.source.write_text("local change\n", encoding="utf-8")
        with self.assertRaisesRegex(RuntimeError, "patch conflicts"):
            PATCHER.apply_patch(self.repo, self.base, self.patch)
        self.assertEqual(self.source.read_text(encoding="utf-8"), "local change\n")


if __name__ == "__main__":
    unittest.main()
