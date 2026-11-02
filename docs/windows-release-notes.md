# AyuGram Desktop 7.3.4 for Windows

This release combines the AyuGram features from the People-11 and PH4N7OMx forks with Telegram Desktop 7.3.4 beta and the subsequent upstream fixes through `5ff615713d2178b8458e201778d23d7abe78db9b`.

## Emoji ID actions

- **Copy with emoji IDs / Копировать с emoji ID** copies a message or selected text with each custom emoji replaced by its decimal Telegram document ID.
- **Copy emoji ID / Копировать emoji ID** copies the ID of the custom emoji under the pointer.
- The actions cover message text, captions, grouped messages, large custom emoji, AyuGram message history and the administrator log.
- Ordinary Unicode emoji remain unchanged: they do not have Telegram custom-emoji document IDs.
- Both actions have bundled translations in all 27 supported locales, including Russian and English. They remain available if the remote language server cannot be reached.
- Copying with emoji IDs also works in the legacy history context menu used by direct chats; emoji pack menus show each owner and ID action once.

## AyuGram features and compatibility

- Includes the applicable People-11 Qt 6 rendering, scrolling and Transfer Boost changes.
- Includes PH4N7OMx message duplicate grouping, extended filters, notification controls, deleted-chat preservation, public-avatar recovery, account-registration estimates and music improvements.
- Preserves AyuGram application names, Windows application ID, local Premium emoji support, appearance settings and message-history features.
- Updates the Telegram base and rebases the AyuGram UI, style generator and syntax-highlighter patches on the pinned dependencies.
- Uses static Qt 6.11.2 for Windows x64.
- Incorporates upstream Windows window-position and fullscreen fixes, WebView camera/microphone permissions, bot-forum link handling, message-list accessibility, sticker ordering, and instant-view copy corrections.

The source-level integration decisions, donor revisions and the reasons for excluding redundant memory9596 changes are recorded in [the integration report](https://github.com/MrVaylis/AyuGramDesktop-windows/blob/dev/docs/INTEGRATION_7_3_4.md).

## Distribution

The Windows packages include a portable ZIP and a per-user installer. The binaries are unsigned. Updates for this fork are installed manually from this repository's releases; Telegram's official updater is disabled.

The build manifest records the source commit, dependency revisions, patch hashes and Windows smoke-test results. Account login and interactive emoji behavior are not part of the unattended smoke test.
