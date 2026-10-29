import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest import mock
import zipfile


SPEC = importlib.util.spec_from_file_location(
    "windows_release", Path(__file__).resolve().parents[1] / "windows_release.py"
)
release = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(release)
SOURCE = "a" * 40


class WindowsReleaseTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        (self.root / "Telegram/build").mkdir(parents=True)
        (self.root / "Telegram/build/version").write_text(
            "AppVersion 7003004\nAppVersionStrSmall 7.3.4\n"
            "BetaChannel 1\nAlphaVersion 0\nAppVersionOriginal 7.3.4.beta\n",
            encoding="utf-8",
        )
        self.environment = {
            "GITHUB_SHA": SOURCE,
            "GITHUB_EVENT_NAME": "push",
            "GITHUB_REF": "refs/heads/dev",
            "GITHUB_RUN_NUMBER": "42",
        }

    def metadata(self, event):
        with mock.patch.object(release, "git", return_value=SOURCE):
            return release.release_metadata(self.root, self.environment, event)

    def test_explicit_dev_release_preserves_beta_channel(self):
        result = self.metadata({"head_commit": {"message": "Publish [release:v7.3.4-ayu.1]"}})
        self.assertEqual(result["publish"], "true")
        self.assertEqual(result["prerelease"], "true")
        self.assertEqual(result["tag"], "v7.3.4-ayu.1")

    def test_normal_dev_build_does_not_publish(self):
        self.assertEqual(self.metadata({})["publish"], "false")

    def test_pull_request_cannot_request_a_release(self):
        self.environment["GITHUB_EVENT_NAME"] = "pull_request"
        result = self.metadata({"head_commit": {"message": "[release:v7.3.4-ayu.1]"}})
        self.assertEqual(result["publish"], "false")

    def test_release_branch_publishes_its_exact_tag(self):
        self.environment["GITHUB_REF"] = "refs/heads/release/v7.3.4-ayu.2"
        self.assertEqual(self.metadata({})["tag"], "v7.3.4-ayu.2")
        self.assertEqual(self.metadata({})["publish"], "true")

    def test_wrong_version_and_shell_syntax_are_rejected(self):
        for tag in ("v7.2.9", "v7.3.4-$(whoami)", "v7.3.4/other", "v7.3.4\nother"):
            with self.subTest(tag=tag), self.assertRaises(ValueError):
                self.environment["GITHUB_REF"] = "refs/tags/" + tag
                self.metadata({})

    def test_dispatch_false_does_not_publish(self):
        self.environment["GITHUB_EVENT_NAME"] = "workflow_dispatch"
        self.assertEqual(self.metadata({"inputs": {"publish_release": "false"}})["publish"], "false")

    def test_wrong_checkout_is_rejected(self):
        self.environment["GITHUB_SHA"] = "b" * 40
        with self.assertRaises(ValueError):
            self.metadata({})

    def test_numeric_version_must_match(self):
        path = self.root / "Telegram/build/version"
        path.write_text(path.read_text().replace("7003004", "7002009"))
        with self.assertRaises(ValueError):
            self.metadata({})

    def prepare_package(self):
        binary_root = self.root / "out/Release"
        (binary_root / "modules/x64/d3d").mkdir(parents=True)
        (binary_root / "AyuGram.exe").write_bytes(b"tested executable")
        (binary_root / "modules/x64/d3d/d3dcompiler_47.dll").write_bytes(b"runtime")
        installer = self.root / "installer.exe"
        installer.write_bytes(b"tested installer")
        for name in ("LICENSE", "LEGAL"):
            (self.root / name).write_text(name)
        metadata = self.metadata({})
        (self.root / "out/windows-metadata.json").write_text(json.dumps(metadata))
        report = self.root / "smoke.json"
        report.write_text(json.dumps({
            "passed": True,
            "sha256": release.digest(binary_root / "AyuGram.exe"),
            "installer_sha256": release.digest(installer),
            "qt_version": "6.11.2",
        }))
        output = self.root / "artifacts"
        with mock.patch.object(release, "git", side_effect=lambda root, *args: SOURCE if args[0] == "rev-parse" else ""), mock.patch.dict(release.os.environ, {"GITHUB_REPOSITORY": "owner/repo", "GITHUB_RUN_ID": "1"}):
            release.package(self.root, output, installer, report)
        return output, metadata

    def test_package_contains_portable_profile_and_runtime(self):
        output, metadata = self.prepare_package()
        release.verify(output, SOURCE, metadata["tag"])
        with zipfile.ZipFile(output / "AyuGram-7.3.4-windows-x64.zip") as archive:
            self.assertIn("AyuGram/TelegramForcePortable/", archive.namelist())
            self.assertIn("AyuGram/modules/x64/d3d/d3dcompiler_47.dll", archive.namelist())
            self.assertEqual(archive.read("AyuGram/AyuGram.exe"), b"tested executable")

    def test_corrupted_binary_is_rejected_before_publish(self):
        output, metadata = self.prepare_package()
        (output / "AyuGram.exe").write_bytes(b"different executable")
        with self.assertRaisesRegex(ValueError, "checksum mismatch"):
            release.verify(output, SOURCE, metadata["tag"])

    def test_wrong_source_and_extra_assets_are_rejected(self):
        output, metadata = self.prepare_package()
        with self.assertRaises(ValueError):
            release.verify(output, "b" * 40, metadata["tag"])
        (output / "unchecked.exe").write_bytes(b"unchecked")
        with self.assertRaisesRegex(ValueError, "unchecked"):
            release.verify(output, SOURCE, metadata["tag"])


if __name__ == "__main__":
    unittest.main()
