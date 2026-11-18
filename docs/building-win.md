# Build instructions for Windows 64-bit

GitHub Actions workflows are disabled. Their definitions are archived in
`.github/workflows-disabled/`. Follow the steps below for local Windows builds.

- [Prepare folder](#prepare-folder)
- [Install third party software](#install-third-party-software)
- [Initialize terminal](#initialize-terminal)
- [Clone source code and prepare libraries](#clone-source-code-and-prepare-libraries)
- [Build the project](#build-the-project)
- [Reuse prepared dependencies and compiler outputs](#reuse-prepared-dependencies-and-compiler-outputs)
- [Troubleshooting](#troubleshooting)

## Prepare folder

The build uses **Visual Studio 2022 17.14** or **Visual Studio 2026** with
**MSVC 14.44** and **Windows SDK 10.0.26100.0**.
The source pins **Qt 6.11.2**; preparation builds the required static libraries.

Install these Visual Studio components:

- MSVC v143 C++ x64/x86 build tools, v14.44-17.14
- C++ MFC v14.44 for x86 and x64
- C++ ATL v14.44 for x86 and x64
- Windows 11 SDK 10.0.26100.0

Choose a build folder, for example `D:\TBuild`. The examples below refer
to it as `BuildPath`. The dependency preparation script creates sibling directories
next to the repository. Windows x64 dependencies live in `Libraries\win64`.

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

## Reuse prepared dependencies and compiler outputs

`out` holds the generated Ninja project and the current build products.
After clearing it, re-run `configure.bat` to reuse the existing
`Libraries\win64` and `ThirdParty` directories. Skip preparation if both
directories already contain the matching dependencies and toolchain.

To configure with prepared directories in other locations, add these arguments
to the same configure command. `AYUGRAM_LIBRARIES_DIR` selects the prepared
libraries even when there is no sibling `Libraries` directory. Use a fresh build
directory when changing the dependency roots so CMake doesn't reuse cached
package paths.

```batch
-D "AYUGRAM_LIBRARIES_DIR=D:/Dependencies/Libraries/win64" ^
-D "AYUGRAM_THIRD_PARTY_DIR=E:/Build Tools/ThirdParty"
```

For a compiler cache that survives recreating `out`, put
[ccache](https://ccache.dev/download.html) 4.10 or newer on `PATH` or place
`ccache.exe` in `ThirdParty\ccache`. Add `-DAYUGRAM_USE_CCACHE=ON` to the
Ninja configure command and set `CCACHE_DIR` in the shell that runs the build.
For example, use `set "CCACHE_DIR=D:\TBuild\.cache\ccache"`.
Use `ccache --show-stats` to inspect hits and misses and
`CCACHE_MAXSIZE` to set the disk limit (5 GiB by default).
Ccache keeps Ninja's precompiled headers enabled. With its default settings,
compilations that use those headers can fall back to MSVC. Ninja still reuses
their object files on incremental builds in the same `out` directory.
To cache those compilations as well, ccache supports
`CCACHE_SLOPPINESS=pch_defines,time_macros`. Use that setting only when the
build can reuse cached macro expansions, including values from `__DATE__`
and `__TIME__`.

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
checkout.
