import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import zipfile


def git(root, *arguments):
    return subprocess.check_output(
        ["git", "-C", str(root), *arguments], text=True, encoding="utf-8"
    ).strip()


def read_version(root):
    values = {}
    for line in (root / "Telegram/build/version").read_text(encoding="utf-8").splitlines():
        if line.strip():
            name, value = line.split(maxsplit=1)
            values[name] = value.strip()
    version = values["AppVersionStrSmall"]
    if not re.fullmatch(r"[0-9]+\.[0-9]+\.[0-9]+", version):
        raise ValueError("Invalid application version")
    major, minor, patch = map(int, version.split("."))
    if int(values["AppVersion"]) != major * 1000000 + minor * 1000 + patch:
        raise ValueError("Numeric and display application versions differ")
    if values["BetaChannel"] not in ("0", "1") or values["AlphaVersion"] != "0":
        raise ValueError("Unsupported release channel")
    return version, values["BetaChannel"] == "1", values["AppVersionOriginal"]


def release_metadata(root, environment, event):
    version, beta, upstream_version = read_version(root)
    source = git(root, "rev-parse", "HEAD")
    if source != environment.get("GITHUB_SHA", source):
        raise ValueError("Checkout does not match the workflow source commit")
    event_name = environment.get("GITHUB_EVENT_NAME", "")
    ref = environment.get("GITHUB_REF", "")
    inputs = event.get("inputs") or {}
    tag = ""
    publish = False
    if event_name == "workflow_dispatch":
        publish = str(inputs.get("publish_release", "false")).lower() == "true"
        tag = inputs.get("release_tag", "").strip()
    elif event_name == "push":
        if ref.startswith("refs/tags/v"):
            publish, tag = True, ref.removeprefix("refs/tags/")
        elif ref.startswith("refs/heads/release/"):
            publish, tag = True, ref.removeprefix("refs/heads/release/")
        elif ref == "refs/heads/dev":
            message = (event.get("head_commit") or {}).get("message", "")
            marker = re.search(r"\[release(?::([^\]\s]+))?\]", message)
            if marker:
                publish, tag = True, marker.group(1) or ""
    if not tag:
        tag = f"v{version}-windows.{environment.get('GITHUB_RUN_NUMBER', 'local')}"
    if not re.fullmatch(r"v" + re.escape(version) + r"(?:[-.][A-Za-z0-9]+)*", tag):
        raise ValueError(f"Release tag must identify source version {version}: {tag}")
    return {
        "version": version,
        "upstream_version": upstream_version,
        "source": source,
        "tag": tag,
        "publish": "true" if publish else "false",
        "prerelease": "true" if beta else "false",
    }


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def package(root, output, installer, smoke_report):
    metadata = json.loads((root / "out/windows-metadata.json").read_text(encoding="utf-8"))
    if metadata["source"] != git(root, "rev-parse", "HEAD"):
        raise ValueError("Source changed after release metadata was prepared")
    smoke = json.loads(smoke_report.read_text(encoding="utf-8"))
    if smoke.get("passed") is not True:
        raise ValueError("Windows smoke test did not pass")
    binary = root / "out/Release/AyuGram.exe"
    if smoke.get("sha256") != digest(binary):
        raise ValueError("Executable changed after the Windows smoke test")
    if smoke.get("installer_sha256") != digest(installer):
        raise ValueError("Installer changed after the Windows smoke test")
    if output.exists() and any(output.iterdir()):
        raise ValueError("Release output directory must be empty")
    output.mkdir(parents=True, exist_ok=True)
    stem = f"AyuGram-{metadata['version']}-windows-x64"
    shutil.copy2(binary, output / "AyuGram.exe")
    shutil.copy2(installer, output / f"{stem}-setup.exe")
    modules = root / "out/Release/modules"
    if not (modules / "x64/d3d/d3dcompiler_47.dll").is_file():
        raise ValueError("Windows D3D compiler runtime is missing")
    submodules = git(root, "submodule", "status", "--recursive").splitlines()
    if any(line.startswith(("-", "+", "U")) for line in submodules):
        raise ValueError("Submodules do not match the source commit")
    patches = {
        path.relative_to(root).as_posix(): digest(path)
        for path in sorted((root / "patches").glob("*.patch"))
    }
    manifest = {
        "application": "AyuGram Desktop",
        "version": metadata["version"],
        "upstream_version": metadata["upstream_version"],
        "tag": metadata["tag"],
        "source_commit": metadata["source"],
        "source_url": f"https://github.com/{os.environ['GITHUB_REPOSITORY']}/tree/{metadata['source']}",
        "architecture": "x64",
        "configuration": "Release",
        "auto_update": False,
        "authenticode_signed": False,
        "qt": smoke["qt_version"],
        "compiler": os.environ.get("VCToolsVersion", ""),
        "windows_sdk": os.environ.get("WindowsSDKVersion", ""),
        "workflow_run": f"https://github.com/{os.environ['GITHUB_REPOSITORY']}/actions/runs/{os.environ['GITHUB_RUN_ID']}",
        "submodules": submodules,
        "applied_patch_sha256": patches,
        "smoke_test": smoke,
    }
    manifest_path = output / "build-manifest.json"
    manifest_path.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    with zipfile.ZipFile(output / f"{stem}.zip", "w", zipfile.ZIP_DEFLATED) as archive:
        archive.write(binary, "AyuGram/AyuGram.exe")
        archive.writestr("AyuGram/TelegramForcePortable/", "")
        archive.write(manifest_path, "AyuGram/build-manifest.json")
        for path in sorted(modules.rglob("*")):
            if path.is_file():
                archive.write(path, "AyuGram/modules/" + path.relative_to(modules).as_posix())
        for name in ("LICENSE", "LEGAL"):
            archive.write(root / name, f"AyuGram/{name}")
        archive.writestr("AyuGram/SOURCE.txt", manifest["source_url"] + "\n")
    release_notes = root / "docs/windows-release-notes.md"
    notes = release_notes.read_text(encoding="utf-8") if release_notes.is_file() else "# AyuGram Desktop for Windows\n"
    notes += (
        f"\n## Build\n\n"
        f"- AyuGram {metadata['version']}; Telegram base {metadata['upstream_version']}.\n"
        f"- Windows x64, static Qt 6, Release configuration.\n"
        f"- Source: {manifest['source_url']}\n"
        f"- Build and smoke test: {manifest['workflow_run']}\n"
        "- Portable ZIP and per-user installer are unsigned. Automatic updates are disabled for this fork.\n"
        "- SHA-256 checksums and exact submodule revisions are included.\n"
    )
    (output / "release-notes.md").write_text(notes, encoding="utf-8")
    checksums = "".join(
        f"{digest(path)}  {path.name}\n" for path in sorted(output.iterdir()) if path.is_file()
    )
    (output / "SHA256SUMS.txt").write_text(checksums, encoding="utf-8")


def verify(output, expected_source, expected_tag):
    manifest = json.loads((output / "build-manifest.json").read_text(encoding="utf-8"))
    if manifest["source_commit"] != expected_source or manifest["tag"] != expected_tag:
        raise ValueError("Downloaded artifacts belong to a different source or release")
    if manifest["smoke_test"].get("passed") is not True:
        raise ValueError("Release lacks a successful Windows smoke test")
    expected_names = {"SHA256SUMS.txt"}
    for line in (output / "SHA256SUMS.txt").read_text(encoding="utf-8").splitlines():
        checksum, name = line.split("  ", 1)
        if Path(name).name != name or name in expected_names or not re.fullmatch(r"[0-9a-f]{64}", checksum):
            raise ValueError("Invalid checksum entry")
        expected_names.add(name)
        if digest(output / name) != checksum:
            raise ValueError(f"Release checksum mismatch: {name}")
    actual_names = {path.name for path in output.iterdir()}
    if actual_names != expected_names:
        raise ValueError("Release contains unchecked files")
    stem = f"AyuGram-{manifest['version']}-windows-x64"
    required = {"AyuGram.exe", f"{stem}.zip", f"{stem}-setup.exe", "build-manifest.json", "release-notes.md"}
    if not required.issubset(expected_names):
        raise ValueError("Release is missing required artifacts")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("command", choices=("metadata", "package", "verify"))
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--output", type=Path)
    parser.add_argument("--installer", type=Path)
    parser.add_argument("--smoke-report", type=Path)
    parser.add_argument("--source")
    parser.add_argument("--tag")
    args = parser.parse_args()
    if args.command == "metadata":
        event = json.loads(Path(os.environ["GITHUB_EVENT_PATH"]).read_text(encoding="utf-8"))
        metadata = release_metadata(args.root, os.environ, event)
        (args.root / "out").mkdir(exist_ok=True)
        (args.root / "out/windows-metadata.json").write_text(json.dumps(metadata, indent=2) + "\n", encoding="utf-8")
        with open(os.environ["GITHUB_OUTPUT"], "a", encoding="utf-8") as stream:
            for key, value in metadata.items():
                stream.write(f"{key}={value}\n")
        print(json.dumps(metadata, indent=2))
    elif args.command == "package":
        package(args.root, args.output, args.installer, args.smoke_report)
    else:
        verify(args.output, args.source, args.tag)


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, KeyError, subprocess.CalledProcessError) as error:
        print(f"Release validation failed: {error}", file=sys.stderr)
        sys.exit(1)
