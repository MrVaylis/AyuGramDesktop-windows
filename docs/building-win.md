# Build instructions for Windows 64-bit

## Windows CI and releases

The single `.github/workflows/windows.yml` workflow builds Windows x64 with
static Qt 6, MSVC 14.44 and Windows SDK 10.0.26100.0 on `windows-2022`.
It runs all build-helper Python tests and the emoji ID, duplicate-group,
registration-estimator and native Prisma regression tests before preparing
Release-only dependencies with `skip-debug`.
Dependency caches include the preparation scripts and exact compiler, SDK and
Python versions in their key. Desktop object files are rebuilt from the checked
out source, including the pinned UI, code generator and Prisma patches.

Every push to `dev` builds a downloadable workflow artifact. To also publish
that exact build, include `[release:v7.3.4-ayu.1]` in the final commit message,
replacing the version with `AppVersionStrSmall` and incrementing the AyuGram
revision for subsequent releases. A bare `[release]` uses a unique tag based on
the source version and workflow run number. A `v*` tag, a branch named
`release/v<version>-ayu.<revision>`, or a manual run with `publish_release`
enabled can also publish. Pull requests never publish releases.

Release publication requires successful compilation, a visible application
window, clean application shutdown, and silent installer and uninstaller tests.
The executable's Windows version resource must identify AyuGram and match the
source version. The workflow publishes `AyuGram.exe`, a portable ZIP, a per-user
installer, SHA-256 checksums, and a manifest identifying the source commit,
submodules, applied UI patch and smoke-test result. The portable ZIP includes
the existing `TelegramForcePortable` directory required by the client.

The publish job verifies checksums and source identity, uploads assets to a
draft, and publishes only after every upload succeeds. It refuses to replace a
published release or a tag referring to another commit. Beta source versions
are published as prereleases. The workflow uses the API identity already
documented by AyuGram; no private signing key is required. These builds are
unsigned and disable automatic updates so the upstream updater cannot replace
this fork. Account login and emoji interactions require a separate signed-in
desktop acceptance check; the automated startup check does not claim them.

- [Prepare folder](#prepare-folder)
- [Install third party software](#install-third-party-software)
- [Initialize terminal](#initialize-terminal)
- [Clone source code and prepare libraries](#clone-source-code-and-prepare-libraries)
- [Build the project](#build-the-project)
- [Troubleshooting](#troubleshooting)

## Prepare folder

The build uses **Visual Studio 2022 17.14** or **Visual Studio 2026** with
**MSVC 14.44** and **Windows SDK 10.0.26100.0**. CI uses Visual Studio 2022.
The source pins **Qt 6.11.2**; preparation builds the required static libraries.

Install these Visual Studio components:

- MSVC v143 C++ x64/x86 build tools, v14.44-17.14
- C++ MFC v14.44 for x86 and x64
- C++ ATL v14.44 for x86 and x64
- Windows 11 SDK 10.0.26100.0

Choose an empty build folder, for example `D:\TBuild`. The examples below refer
to it as `BuildPath`. The repository, `Libraries` and `ThirdParty` must share
that parent folder. Windows x64 dependencies live in `Libraries\win64`.

## Install third party software

- Install **Python 3.12** or newer from [python.org](https://www.python.org/downloads/) and add it to PATH.
- Install **Git** from [git-scm.com](https://git-scm.com/download/win).
- Install **CMake 3.31** and **Ninja 1.13** or newer and add both to PATH.

## Initialize terminal

Use an x64 Native Tools terminal for preparation and configuration. With
Visual Studio 2026, select the v14.44 toolset explicitly; its default v145
compiler does not preserve the client baseline used by these builds.

For Visual Studio 2022 Community:

```batch
%comspec% /k "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" -vcvars_ver=14.44 10.0.26100.0
```

For Visual Studio 2026 Build Tools:

```batch
%comspec% /k "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" -vcvars_ver=14.44 10.0.26100.0
```

Adjust the installation path for Enterprise or another installed edition.

## Clone source code and prepare libraries

From `BuildPath`, run each command and stop if it reports an error:

```batch
git clone --branch dev --recursive https://github.com/MrVaylis/AyuGramDesktop-windows.git tdesktop
cd tdesktop
python Telegram\build\apply_ayu_patches.py
python Telegram\build\apply_ayu_patches.py --check
Telegram\build\prepare\win.bat silent qt6 skip-debug
```

The patch helper checks exact submodule revisions, applies the AyuGram UI and
language-generator changes, and limits Prisma recursion. Run it again after
updating submodules; repeated application is safe. Preparation creates the
`Libraries` and `ThirdParty` sibling directories automatically.

## Build the project

From `BuildPath\tdesktop\Telegram`, run:

```batch
configure.bat -G "Ninja Multi-Config" qt6 ^
  -D TDESKTOP_API_ID=2040 ^
  -D TDESKTOP_API_HASH=b18441a1ff607e10a989891a5462e627 ^
  -D CMAKE_CONFIGURATION_TYPES=Release ^
  -D DESKTOP_APP_DISABLE_AUTOUPDATE=ON ^
  -D DESKTOP_APP_DISABLE_CRASH_REPORTS=ON ^
  -D DESKTOP_APP_ENABLE_LTO=OFF
cmake --build ..\out --config Release --target Telegram --parallel 2
```

The result is `BuildPath\tdesktop\out\Release\AyuGram.exe`. Keep its `modules`
directory when distributing the executable. The Ninja generator inherits the
x64 compiler from the Native Tools terminal; do not add the `x64` positional
argument used by the Visual Studio generator.

For a Debug build, prepare dependencies without `skip-debug` and configure a
separate checkout or build directory for Debug. The release-only cache omits
Debug libraries.

## Troubleshooting

### Missing libraries or the wrong architecture

Use the same x64 Native Tools terminal for preparation and configuration. Check
that the repository has `Libraries` and `ThirdParty` siblings and that
`Libraries\win64\Qt-6.11.2` exists. A source checkout alone does not provide the
compiled dependencies.

### Failed dependency preparation

Keep the full command output and rerun `win.bat silent qt6 skip-debug` after
fixing the reported error. Completed stages have content-checked cache markers;
incomplete stages are rebuilt. Use the dependency versions pinned by this
checkout. CI preserves diagnostic artifacts when a later client build or
startup test fails.
