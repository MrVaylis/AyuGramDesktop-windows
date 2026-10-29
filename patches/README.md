# AyuGram dependency patches

Run `python Telegram/build/apply_ayu_patches.py` from the repository root after initializing submodules and before configuring CMake. Repeating the command is safe; `--check` verifies that all patches are already applied. The helper rejects different dependency revisions and conflicting local changes.

| Dependency | Upstream commit | Local changes |
| --- | --- | --- |
| lib_ui | `09c7db5ca60ac5537e9f7ed8808a833f93b8faf2` | AyuGram UI settings, local emoji entities, streamer menu hook, wide chat styling, fonts and controls; People-11 Windows frame pacing, smooth scrolling, and asynchronous emoji compression. |
| codegen | `dbd1d53137cd581cfdbeebe93bcaf54abf55ec74` | AyuGram language-key scanning, cache version 2, and runtime wide-chat style values. |
| libprisma | `086296336c69723f58ca6aa603434ef8cf682c39` | The PH4N7OMx grammar recursion limit also bounds greedy rematches; the upstream tokenization guard is retained. |

`lib_ui.patch` is a combined patch against the official lib_ui revision. It includes the AyuGram changes from `e3d058223aea0b68938a07a7d436d67aaf2b1b30` and the applicable People-11 `qt6-rhi` changes from `6241b491e54602166e9a5e8d8052b6dff342aeaf`. Upstream already enables Windows Qt RHI and implements Qt 6 custom window margins; those upstream implementations remain intact. Translucent-window handling is limited to Windows.

`ayu-codegen.patch` carries the AyuGram generator changes from `be951ec045da4f3a6dc271a758ce50c2c45a6d8a` onto the current upstream generator. Prisma is patched by `Telegram/build/patch_prisma.py`, preserving the newer embedded-language support.

When changing a dependency pin, rebase its patch against the new upstream source and update the expected revision in the helper. Verify a clean application, a second idempotent application, and `--check`. Run the Python tests in `Telegram/build/tests` and the native Prisma regression test (`Telegram/build/tests/run_prisma_depth_test.py`) before the Windows build.
