#pragma once

#include "ui/text/text_entity.h"

#include <cstdint>

namespace Ayu {

[[nodiscard]] std::uint64_t FirstEmojiId(const TextWithEntities &text);
[[nodiscard]] QString TextWithEmojiIds(const TextWithEntities &text);

} // namespace Ayu
