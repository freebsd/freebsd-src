#include <wctype.h>

int iswalnum(wint_t wc)
{
	if (iswdigit(wc)) return 1;
	return iswalpha(wc);
}

int __iswalnum_l(wint_t c, locale_t l)
{
	return iswalnum(c);
}

weak_alias(__iswalnum_l, iswalnum_l);
