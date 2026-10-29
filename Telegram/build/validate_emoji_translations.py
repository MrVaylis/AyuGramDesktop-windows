#!/usr/bin/env python3

import json
import re
from pathlib import Path


def main():
    telegram = Path(__file__).resolve().parents[1]
    resources = telegram / "Resources"
    translations = json.loads(
        (resources / "langs/ayu_local.json").read_text(encoding="utf-8")
    )
    languages = {
        "ar", "be", "de", "el", "en", "eo", "es", "fa", "fi", "fr", "he",
        "it", "ja", "ko", "lv", "nl", "os", "pl", "pt", "ro", "ru", "tr",
        "uk", "vi", "zh", "zh-hans", "zh-hant",
    }
    keys = {"CopyWithEmojiId", "CopyEmojiId"}
    if languages - translations.keys():
        raise ValueError(f"Missing languages: {languages - translations.keys()}")
    for language, values in translations.items():
        if set(values) != keys:
            raise ValueError(f"Incorrect translation keys for {language}")
        for key, value in values.items():
            if not isinstance(value, str) or not value.strip():
                raise ValueError(f"Empty translation: {language}/{key}")
            if any(character in value for character in ("\n", "\r", "\x00")):
                raise ValueError(f"Invalid menu text: {language}/{key}")
    source = (resources / "langs/lang.strings").read_text(encoding="utf-8")
    for key, value in translations["en"].items():
        matches = re.findall(rf'"ayu_{key}"\s*=\s*"([^"]+)";', source)
        if matches != [value]:
            raise ValueError(f"English source mismatch: {key}")
    qrc = (resources / "qrc/ayu/ayu.qrc").read_text(encoding="utf-8")
    if 'alias="langs/ayu_local.json"' not in qrc:
        raise ValueError("Bundled translations are missing from the Qt resources")
    print(f"Emoji ID translations: {len(translations)} languages, {len(keys)} keys PASS")


if __name__ == "__main__":
    main()
