#pragma once

#include <algorithm>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace Ayu::EmojiIds {

struct Replacement {
	int offset = 0;
	int length = 0;
	std::uint64_t id = 0;
};

[[nodiscard]] inline std::uint64_t ParseId(std::u16string_view text) {
	if (text.empty()) {
		return 0;
	}
	auto result = std::uint64_t(0);
	for (const auto character : text) {
		if (character < u'0' || character > u'9') {
			return 0;
		}
		const auto digit = std::uint64_t(character - u'0');
		if (result > (std::numeric_limits<std::uint64_t>::max() - digit) / 10) {
			return 0;
		}
		result = result * 10 + digit;
	}
	return result;
}

[[nodiscard]] inline bool IsValid(
		const Replacement &replacement,
		std::size_t size) {
	return replacement.id
		&& replacement.offset >= 0
		&& replacement.length > 0
		&& std::size_t(replacement.offset) <= size
		&& std::size_t(replacement.length) <= size - replacement.offset;
}

[[nodiscard]] inline std::u16string Replace(
		std::u16string_view text,
		std::vector<Replacement> replacements) {
	std::stable_sort(
		replacements.begin(),
		replacements.end(),
		[](const Replacement &a, const Replacement &b) {
			return a.offset < b.offset;
		});
	auto result = std::u16string();
	auto cursor = std::size_t(0);
	for (const auto &replacement : replacements) {
		if (!IsValid(replacement, text.size())
			|| std::size_t(replacement.offset) < cursor) {
			continue;
		}
		const auto offset = std::size_t(replacement.offset);
		result.append(text.substr(cursor, offset - cursor));
		for (const auto digit : std::to_string(replacement.id)) {
			result.push_back(char16_t(digit));
		}
		cursor = offset + replacement.length;
	}
	result.append(text.substr(cursor));
	return result;
}

} // namespace Ayu::EmojiIds
