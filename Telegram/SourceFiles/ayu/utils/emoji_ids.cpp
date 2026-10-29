#include "ayu/utils/emoji_ids.h"

#include "ayu/utils/emoji_id_replacements.h"

namespace Ayu {
namespace {

[[nodiscard]] EmojiIds::Replacement ReplacementFor(
		const EntityInText &entity) {
	return {
		.offset = entity.offset(),
		.length = entity.length(),
		.id = (entity.type() == EntityType::CustomEmoji)
			? EmojiIds::ParseId(entity.data().toStdU16String())
			: 0,
	};
}

} // namespace

std::uint64_t FirstEmojiId(const TextWithEntities &text) {
	for (const auto &entity : text.entities) {
		const auto replacement = ReplacementFor(entity);
		if (EmojiIds::IsValid(replacement, text.text.size())) {
			return replacement.id;
		}
	}
	return 0;
}

QString TextWithEmojiIds(const TextWithEntities &text) {
	auto replacements = std::vector<EmojiIds::Replacement>();
	for (const auto &entity : text.entities) {
		const auto replacement = ReplacementFor(entity);
		if (EmojiIds::IsValid(replacement, text.text.size())) {
			replacements.push_back(replacement);
		}
	}
	return QString::fromStdU16String(EmojiIds::Replace(
		text.text.toStdU16String(),
		std::move(replacements)));
}

} // namespace Ayu
