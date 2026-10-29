#include "ayu/utils/emoji_id_replacements.h"

#include <cstdlib>
#include <iostream>

namespace {

void Check(bool condition, const char *name) {
	if (!condition) {
		std::cerr << "FAIL: " << name << '\n';
		std::exit(1);
	}
}

} // namespace

int main() {
	using namespace Ayu::EmojiIds;

	Check(ParseId(u"5368324170671202286") == 5368324170671202286ULL,
		"64-bit document ID keeps every digit");
	Check(ParseId(u"18446744073709551615") == UINT64_MAX,
		"maximum unsigned document ID");
	for (const auto invalid : {
			u"", u"0", u"-1", u"+1", u" 123", u"123 ",
			u"18446744073709551616", u"123abc", u"userpic:123", u"١٢٣" }) {
		Check(ParseId(invalid) == 0, "reject malformed document ID");
	}
	Check(ParseId(u"00042") == 42, "normalize leading zeros");
	Check(Replace(u"hello 😊", {}) == u"hello 😊",
		"ordinary Unicode emoji stay unchanged");
	Check(Replace(u"A😀B💙C", { { 4, 2, 22 }, { 1, 2, 11 } })
		== u"A11B22C", "sort entities without shifting UTF-16 offsets");
	Check(Replace(u"😀😀", { { 0, 2, 18446744073709551615ULL }, { 2, 2, 2 } })
		== u"184467440737095516152", "adjacent emoji IDs and full precision");
	Check(Replace(u"👨‍👩‍👧‍👦!", { { 0, 11, 42 } }) == u"42!",
		"replace the complete multi-codepoint emoji span");
	Check(Replace(u"Привет 😀\nالعربية 💙", { { 7, 2, 71 }, { 18, 2, 72 } })
		== u"Привет 71\nالعربية 72", "Unicode text and line breaks survive");
	Check(Replace(u"😀🌟", { { 0, 2, 9 } }) == u"9🌟",
		"mixed custom and ordinary emoji");
	Check(Replace(u"😀x", {
		{ -1, 2, 1 }, { 0, -1, 2 }, { 0, 0, 3 }, { 0, 2, 0 },
		{ 0, 4, 4 }, { 4, 1, 5 }, { 0, 2, 6 }, { 0, 2, 7 }, { 1, 2, 8 },
	}) == u"6x", "ignore invalid, duplicated, and overlapping ranges");
	Check(Replace(u"😀 x 😀", { { 0, 2, 88 }, { 5, 2, 88 } })
		== u"88 x 88", "repeated document IDs are copied at each occurrence");
	Check(Replace(u"", { { 0, 1, 1 } }).empty(), "empty text");
	Check(Replace(u"x", { { 0, 2147483647, 1 } }) == u"x",
		"large malformed length cannot overflow bounds");

	std::cout << "Emoji ID replacement regression cases PASS\n";
	return 0;
}
