#include "lt_types.h"

typedef struct wchar_range { wchar first, last; } wchar_range;

static wchar_range wide_chars[] = {
#include "../wide.uni"
};
static wchar_range compose_table[] = {
#include "../compose.uni"
};
static wchar_range fmt_table[] = {
#include "../fmt.uni"
};
static wchar_range comb_table[] = {
	{0x0644,0x0622}, {0x0644,0x0623}, {0x0644,0x0625}, {0x0644,0x0627},
};

static int is_in_table(wchar ch, wchar_range table[], int count) {
	if (ch < table[0].first)
		return 0;
	int lo = 0;
	int hi = count - 1;
	while (lo <= hi) {
		int mid = (lo + hi) / 2;
		if (ch > table[mid].last)
			lo = mid + 1;
		else if (ch < table[mid].first)
			hi = mid - 1;
		else
			return 1;
	}
	return 0;
}

int is_wide_char(wchar ch) {
	return is_in_table(ch, wide_chars, countof(wide_chars));
}

int is_composing_char(wchar ch) {
	return is_in_table(ch, compose_table, countof(compose_table)) ||
	       is_in_table(ch, fmt_table, countof(fmt_table));
}

int is_combining_char(wchar ch1, wchar ch2) {
	int i;
	for (i = 0; i < countof(comb_table); i++) {
		if (ch1 == comb_table[i].first &&
		    ch2 == comb_table[i].last)
			return 1;
	}
	return 0;
}
