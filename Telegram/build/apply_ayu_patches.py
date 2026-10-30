import argparse
from pathlib import Path
import subprocess
import tempfile

from patch_prisma import main as patch_prisma


PATCHES = (
    (
        "Telegram/lib_ui",
        "09c7db5ca60ac5537e9f7ed8808a833f93b8faf2",
        "patches/lib_ui.patch",
    ),
    (
        "Telegram/codegen",
        "dbd1d53137cd581cfdbeebe93bcaf54abf55ec74",
        "patches/ayu-codegen.patch",
    ),
)


def git(folder, *args, check=True):
    return subprocess.run(
        ["git", "-C", str(folder), *args],
        check=check,
        capture_output=True,
        text=True,
        encoding="utf-8",
    )


def apply_patch(folder, base, patch, check_only=False):
    with tempfile.TemporaryDirectory(prefix="ayu-patch-") as directory:
        normalized = Path(directory) / patch.name
        normalized.write_bytes(patch.read_bytes().replace(b"\r\n", b"\n"))
        return apply_normalized_patch(folder, base, normalized, check_only)


def apply_normalized_patch(folder, base, patch, check_only=False):
    actual = git(folder, "rev-parse", "HEAD").stdout.strip()
    if actual != base:
        raise RuntimeError(f"{folder.name}: expected base {base}, found {actual}.")
    reverse = git(folder, "apply", "--reverse", "--check", str(patch), check=False)
    if reverse.returncode == 0:
        return "already applied"
    if check_only:
        raise RuntimeError(f"{folder.name}: required patch is not applied: {patch.name}.")
    forward = git(folder, "apply", "--check", str(patch), check=False)
    if forward.returncode:
        raise RuntimeError(
            f"{folder.name}: patch conflicts with the checkout: {patch.name}.\n"
            + forward.stderr.strip()
        )
    git(folder, "apply", "--whitespace=nowarn", str(patch))
    git(folder, "apply", "--reverse", "--check", str(patch))
    return "applied"


def main():
    parser = argparse.ArgumentParser(description="Apply the pinned AyuGram dependency patches.")
    parser.add_argument("--check", action="store_true", help="Verify patches without changing files.")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    for relative, base, patch in PATCHES:
        state = apply_patch(root / relative, base, root / patch, args.check)
        print(f"{relative}: {state}")
    prisma_root = root / "Telegram/ThirdParty/libprisma"
    prisma_base = "086296336c69723f58ca6aa603434ef8cf682c39"
    actual = git(prisma_root, "rev-parse", "HEAD").stdout.strip()
    if actual != prisma_base:
        raise RuntimeError(f"libprisma: expected base {prisma_base}, found {actual}.")
    if args.check:
        from patch_prisma import patch_sources

        prisma = root / "Telegram/ThirdParty/libprisma/libprisma"
        files = (prisma / "SyntaxHighlighter.h", prisma / "SyntaxHighlighter.cpp")
        original = tuple(path.read_text(encoding="utf-8") for path in files)
        if patch_sources(*original) != original:
            raise RuntimeError("libprisma: required recursion limit is not applied.")
    else:
        patch_prisma()


if __name__ == "__main__":
    main()
