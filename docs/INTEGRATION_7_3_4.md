# AyuGram Desktop 7.3.4 integration checkpoint

## Status and scope

**Prepared source checkpoint; Windows build and release are pending.** This report records the integration reviewed on 2026-10-29 UTC. The reviewed source snapshot is `6f2c3140cc85da62484fdeb9b9558a69be0586be`; the documentation commit containing this report is a subsequent checkpoint.

The target is [MrVaylis/AyuGramDesktop-windows, branch `dev`](https://github.com/MrVaylis/AyuGramDesktop-windows/tree/dev). No remote write, Windows Actions build, Windows executable, installer, or published release was completed during this integration session. The connected GitHub integration returned HTTP 403 on the first attempted Git blob creation for the target repository; its available installations/accounts did not include `MrVaylis`. A Git push dry run also had no CLI credentials. These are delivery/access blockers, separate from the local source and regression checks below.

This report describes source decisions and observed checks. It does not assert that the complete client has compiled or passed Windows runtime validation.

## Source revisions and version

| Source | Exact revision | Integration decision |
| --- | --- | --- |
| Original target `dev`, identical to the inspected AyuGram `dev` | [`0f2a31bb420de98f331990432265b131b8d836b5`](https://github.com/MrVaylis/AyuGramDesktop-windows/commit/0f2a31bb420de98f331990432265b131b8d836b5) | Starting point, version 7.2.9. |
| PH4N7OMx `dev` | [`944ffa75d55e9c8a1c6d10d0414519262f46e3e5`](https://github.com/PH4N7OMx/AyuGramDesktop/commit/944ffa75d55e9c8a1c6d10d0414519262f46e3e5) | Full branch history merged; conflicting final changes reconciled with current AyuGram. |
| People-11 `qt6-rhi` | [`6241b491e54602166e9a5e8d8052b6dff342aeaf`](https://github.com/People-11/AyuGramDesktop/commit/6241b491e54602166e9a5e8d8052b6dff342aeaf) | Full branch history merged; Qt/UI dependency changes rebased onto the newer official library. |
| memory9596 `dev` | [`51adc2dc81ec71f9d4837c94beba6e8fce663686`](https://github.com/memory9596/AyuGramDesktop/commit/51adc2dc81ec71f9d4837c94beba6e8fce663686) | Reviewed; no separate cherry-pick required. Decisions are recorded below. |
| Official Telegram Desktop `dev` | [`f1309a2456fb13161f8dc28984d7dec8d6cd8390`](https://github.com/telegramdesktop/tdesktop/commit/f1309a2456fb13161f8dc28984d7dec8d6cd8390) | Latest official development snapshot inspected for this checkpoint, merged with AyuGram behavior and identity retained. |

Relative to the original target, PH4N7OMx had 208 ahead / 10 behind commits and People-11 had 11 ahead / 2 behind commits. Those counts include merges, build changes, fixes, and reverted experiments; they are not counts of distinct delivered features.

The official snapshot is **10 commits after** official tag [`v7.3.4`](https://github.com/telegramdesktop/tdesktop/releases/tag/v7.3.4), whose commit is `0771a62cdc10470cd5f3b61601cc251b019e42c0`. Both the tag and the inspected development snapshot declare **7.3.4 beta**. The integration therefore uses:

- `AppVersion = 7003004` and display version `7.3.4`.
- `BetaChannel = 1` and `AppVersionOriginal = 7.3.4.beta`.
- Windows file/product version `7.3.4.0`.

The version is taken from the inspected upstream source. No unannounced 7.3.5 version or stable-release status is assigned. Future upstream changes after `f1309a2` are outside this checkpoint.

### Integration history

The donor merge commits are `4ea83cce782bf090f008b6c838f9b7384b14d632` (PH4N7OMx) and `bfe57b3faa1633ada21e06811766560a2bb05c67` (People-11). The official update was prepared in `5a0118e8d71fa526ba3b0b37097772d1c42e3bea`; `6e4f69227610d1c16a1378e92afd4e6e2b563866` combines it with the donors. Subsequent commits add emoji actions, bundled translations, dependency rebasing, current-API compatibility fixes, and the single Windows workflow. The merge graph retains the selected donor and official source histories for review.

## Functional integration decisions

### PH4N7OMx

The final source retains these feature groups from the donor:

- Duplicate-message grouping, display counts, expanded filter scopes, and notification/mute controls.
- Retention of removed chats and associated history behavior.
- Public-avatar resolution and profile presentation changes.
- Account registration information that distinguishes known metadata from estimates, including the bounded local estimation model.
- Message screenshot rendering, saved-music/profile UI improvements, and settings persistence/backup improvements.
- Language cache/loading improvements, dependency-preparation options, and regression fixtures.

The merge preserves newer AyuGram behavior that predates this integration: unsafe-URL handling, local premium emoji flags, custom message-time formatting, AyuUiSettings, avatar corner/font/control settings, and the original application's identity. Read-state changes retain the current TTL-media conditions. The newer Telegram selected-copy and profile APIs were adapted while retaining the Ayu summary-copy and saved-music behavior.

The donor's promotional first-run channel popup was omitted. Its release workflows were replaced by the unified target-repository workflow. Donor dependency pointers were not blindly substituted: the PH `lib_tl` tree was equivalent to the retained Ayu revision, its codegen variant dropped existing `ayu_` scanning, and the existing Ayu UI integration provided the required streamer-menu hook.

Integration fixes also bound language fallback to one applicable retry, dispose aborted/completed network replies, preserve current translation-placeholder names, and avoid using a user pointer after avatar-change callbacks can invalidate it. These are source-level fixes; their complete GUI/network behavior remains part of the pending runtime review.

### People-11

The integration retains the applicable final source changes for:

- Windows Qt 6 rendering, smooth wheel scrolling, display-paced animation scheduling, and asynchronous emoji-image compression.
- Transfer Boost download/upload scheduling changes.
- Local repeating-message scheduling, repeat icons, unread-channel swipe behavior, and viewing all replies.
- Forwarding without an unwanted quote and excluding group bots when the relevant saving option is disabled.
- Updated MSYS2/UCRT preparation.

Existing message-selection actions remain available. The donor's removal of those actions was not required for the new reply action. The PH preparation script's explicit `skip-debug` behavior was retained and adapted to the current upstream preparation flow; the donor's blanket Debug-to-MinSizeRel rewrite was not used.

The old People lib_ui patch was rebuilt against the current official lib_ui revision. Upstream already enables Windows Qt RHI and handles Qt 6 custom window margins, so those newer implementations remain. Translucent-window handling is scoped to Windows. A smooth-scroll callback no longer accesses the animation object after a callback can destroy it.

### Requested emoji ID actions

The source adds **Copy with emoji IDs** and **Copy emoji ID**; the Russian labels are **Копировать с emoji ID** and **Копировать emoji ID**.

The first action replaces custom-emoji entity spans with their decimal Telegram document IDs in copied plain text. It uses the same message/group/selection content as the corresponding copy path, including captions and albums where applicable. Ordinary Unicode emoji remain text because they have no custom-emoji document ID. The standard copy command remains available.

Both actions are also available in the retained message-edit history and administrator-log viewers. The second action resolves the custom emoji under the context-menu cursor, including enlarged custom-emoji message rendering. It copies that emoji's ID, rather than choosing an unrelated emoji elsewhere in the message. Invalid, zero, overflowing, overlapping, or out-of-range replacement metadata is rejected or skipped. UTF-16 offsets are preserved. In the main history menu, existing copy-restriction checks are reused at menu creation and action execution; the edit-history and administrator-log integrations follow their existing copy paths.

The two labels are bundled in `Telegram/Resources/langs/ayu_local.json` for all **27 supported language entries**, including English and Russian. The Qt resource loads these labels independently of remote language-pack availability; locale/base-language fallback is supported, and downloaded packs are followed by the bundled overrides for these keys. The general Ayu translation loader still uses the imported PH language source for other donor strings; the new emoji labels do not depend on that service being reachable.

Primary implementation paths are `ayu/utils/emoji_id_replacements.h`, `ayu/utils/emoji_ids.cpp`, the history context menu and cursor state, custom-emoji rendering, `ayu/ui/message_history/history_inner.cpp`, `history/admin_log/history_admin_log_inner.cpp`, and `ayu/ayu_lang.cpp`, all under `Telegram/SourceFiles`.

## Why no additional memory9596 commits were selected

All six ahead commits were inspected. Applying the branch wholesale would duplicate already integrated work and introduce a competing CI path.

| Commit | Change class | Decision and evidence |
| --- | --- | --- |
| `b876153264944302bb1dbacd196cc0337a9fc4d7` | Previous media in the administrator log | Already covered by `history/admin_log/history_admin_log_item.cpp`: it obtains `prevPhoto` / `prevDocument` and passes them to `addLogEntryOriginal`. The older patch introduces another old-message/media item and a repeated original-entry path, creating duplicate previous-media presentation. Omitted. |
| `d65c58fbb954d5ca9d0aeac05266d365ad2598d1` | `skip-debug` preparation option | Superseded by the retained PH preparation options and subsequent adaptations to the new upstream library preparation commands. The release workflow explicitly uses `skip-debug`; its behavior has unit coverage. |
| `870ba25f82adcf6bee0df0067b3e35708cca5bb6` | Avoid Windows `dump_syms` generation | The retained preparation code already supports the relevant symbol-skipping behavior. The custom Windows build also disables crash reports; no separate older CI workaround is required. |
| `34f4931208fd025ce45bd7a6c5570310ce1f207d` | Standalone Windows x64 CI | Replaced by `.github/workflows/windows.yml`, which prepares dependencies, builds, validates, packages, and conditionally publishes from one workflow. |
| `afb49bb089059516604e49359e70fab523ee2860` | MSYS2/UCRT packages | Covered by the selected People changes in the retained preparation script. |
| `51adc2dc81ec71f9d4837c94beba6e8fce663686` | Transfer Boost | Already obtained through People-11 `1f93a1b423fceead5b61183cd3c9a8100c03ce56`; a second application is unnecessary. |

Paths in this table are relative to `Telegram/SourceFiles` unless otherwise specified. These omissions remove duplicate/superseded changes; they do not defer an independent memory9596-only feature.

## Dependency pins and compatibility patches

The gitlinks remain the authoritative dependency revisions. The release manifest generator records recursive submodule revisions plus patch/helper SHA-256 hashes when a real build is packaged.

| Dependency path | Pinned revision | Local handling |
| --- | --- | --- |
| `Telegram/lib_ui` | `09c7db5ca60ac5537e9f7ed8808a833f93b8faf2` | Official base plus `patches/lib_ui.patch`, combining retained Ayu UI behavior and applicable People changes. |
| `Telegram/codegen` | `dbd1d53137cd581cfdbeebe93bcaf54abf55ec74` | Official base plus `patches/ayu-codegen.patch`: `ayu_` scanning, cache version 2, and runtime wide-chat style values. |
| `Telegram/ThirdParty/libprisma` | `086296336c69723f58ca6aa603434ef8cf682c39` | Patched by `Telegram/build/patch_prisma.py` to bound greedy rematches and nested grammar work. The upstream tokenization guard and embedded-language support remain. |
| `Telegram/lib_tl` | `eb38efe1447ae02883f9619ec899b4fe21309b95` | Retained Ayu revision. The inspected PH `e61a6ade91ce0525836d4a317d0e41265d9be2f8` has an identical tree. |
| `cmake` | `7ceca3019b4750d7a5acb2de0faffd523ee14095` | Current official build infrastructure. |
| `Telegram/lib_base` | `42a0e5dd634e638de7115e5a4cfcdd72786853a3` | Pinned in the integrated source. |
| `Telegram/lib_crl` | `de724667d7bdfbf906f78fc405af1ee9f7723d49` | Pinned in the integrated source. |
| `Telegram/lib_rpl` | `c57cccffb01d85570decd7fccb88419c9a682e63` | Pinned in the integrated source. |
| `Telegram/ThirdParty/tgcalls` | `1c236c09f8d8569fead14bd68000618a52051225` | Pinned in the integrated source. |

The single entry point is:

```sh
python Telegram/build/apply_ayu_patches.py
python Telegram/build/apply_ayu_patches.py --check
```

It checks the exact lib_ui, codegen, and Prisma base commits, applies only the required patches, supports an idempotent repeat, and rejects conflicting local edits. No unpublished donor library commit is required. See [`patches/README.md`](../patches/README.md) for maintenance details. Other direct and nested dependencies remain pinned by their gitlinks; this table highlights the integration-sensitive libraries rather than duplicating the complete recursive manifest.

## AyuGram identity and update policy

The updated Telegram version does not replace the application identity. The reviewed sources retain:

- Product/application name `AyuGram Desktop` and executable `AyuGram.exe`.
- Existing Windows AppId `{53F49750-6209-4FBF-9CA8-7A333C87D666}` in `core/version.h` and `Telegram/build/setup.iss`.
- `Radolyn Labs` Windows resource/installer branding, AyuGram icons, and AyuGram paths/identifiers.
- The existing `one.ayugram.AyuGramDesktop` and `com.ayugram.desktop` platform identifiers.

Automatic updating is **forced off** in the root CMake configuration and explicitly disabled by the Windows workflow. The newer upstream updater implementation remains in the source tree, but these custom builds do not consume Telegram's or another fork's binary-update packages. The installer omits `Updater.exe` in this configuration. Updates for this Windows fork are installed manually from its successfully published release assets. No custom signed-update infrastructure is claimed to exist.

## Unified Windows build and publication path

`.github/workflows/windows.yml` is the only workflow in this checkpoint. It defines a Windows x64 build on `windows-2022` with static Qt 6, MSVC, pinned source/submodules/actions, release-only dependency preparation, and a separate publication job gated on successful build output.

The prepared workflow runs helper/feature regressions, applies and verifies the dependency patches, builds Release, checks the executable's PE architecture/version/AyuGram product identity, and runs startup/clean-shutdown and installer/uninstaller smoke checks. Successful packaging is designed to produce the executable, portable ZIP, per-user installer, checksum file, and manifest. The publication job validates source/tag/checksums, uploads to a draft, and publishes after successful verification. Existing published releases are not overwritten.

A normal `dev` push builds without automatically publishing. Publication is requested by an eligible version tag, `release/**` branch, `[release]` / `[release:TAG]` in a `dev` commit, or explicit workflow-dispatch inputs. The retained beta flag makes the resulting GitHub release a prerelease. Artifacts are configured as unsigned; no Authenticode signature is asserted.

Final source review caught duplicate `OutputBaseFilename` installer directives that would conflict with the workflow's requested artifact name. Commit `6f2c3140cc85da62484fdeb9b9558a69be0586be` removes the two old overrides; 12 static preprocessor combinations were checked. ISCC itself has not run. The optional settings-description subscription was also restored to remain optional in `82612691f68b2b8a41a3abf37bda64fafffe9c34`.

These are implemented workflow gates, **not completed Windows test results**. The smoke report explicitly distinguishes startup/install checks from account-login and interactive emoji checks; it does not mark those latter checks as passed.

## Observed local validation

The following checks completed in the local Linux workspace. The Python and native non-Qt checks were run against the integrated source; dependency application and Prisma tests used the exact pinned dependency revisions.

| Check | Observed result | Boundary |
| --- | --- | --- |
| `python3 -m unittest discover -s Telegram/build/tests -p 'test_*.py'` | 18 tests passed. | Patch application, codegen helper, and release-metadata/artifact validation logic. |
| `python3 -m unittest discover -s Telegram/build/prepare/tests -p 'test_*.py'` | 6 tests passed. | Preparation-option and release-only command transformations. |
| `python3 Telegram/build/validate_emoji_translations.py` | 27 languages, 2 keys passed. | Bundled labels, key completeness, English source agreement, resource inclusion. |
| `tests/test_emoji_ids.cpp`, compiled with GCC C++20, warnings as errors | Passed. | Decimal ID parsing and UTF-16 replacement cases; no Qt GUI interaction. |
| `ayu/features/filters/tests/run_duplicate_group_test.py` | Passed. | Authors, previews, counts, signatures, hidden authors, replies, and text grouping. |
| `ayu/utils/tests/registration_model_test.cpp`, compiled with GCC C++20, warnings as errors | Passed. | Registration-estimation model cases. |
| Clean lib_ui/codegen checkout: apply, repeat, and `--check` | Passed. | Exact patch compatibility and idempotence at the pinned revisions. |
| Unified dependency helper: two runs and `--check` in the integrated checkout | Passed. | Integrated patch installation; source compilation is separate. |
| `Telegram/build/tests/run_prisma_depth_test.py` with current pinned Prisma and GCC C++20 | Passed. | Cyclic grammar, depth cutoff, text preservation, and repeated destruction; MSVC execution is pending. |
| Installer output-name branches | 12 static combinations passed. | No Inno Setup compilation or installation was performed locally. |
| Git whitespace check on edited source/build/documentation files | Passed. | Unified patch context lines are excluded from blanket whitespace linting. |

## Remaining delivery and runtime work

1. Restore a GitHub connection with write/Actions/release access to the actual target repository and publish the reviewed source commit.
2. Run the Windows workflow on that exact commit. Resolve any real MSVC, dependency-preparation, packaging, or startup failure before creating a published release.
3. Perform an interactive Windows check of both emoji actions with multiple custom emoji, Unicode emoji, captions/albums, selections, and enlarged emoji. Confirm language switching and copy restrictions in the real UI.
4. Exercise the integrated settings, avatar/music/history features, and Windows frame pacing on supported hardware. The narrow regression checks above do not establish performance or end-to-end account behavior.
5. Publish only the artifacts tied to the successful workflow's exact source commit, manifest, checksums, and smoke report.

At this checkpoint there is no successful Actions URL, downloadable Windows release, Authenticode signature, or account-login result to cite. The source and workflow are ready for that next validation stage once repository access is available.
