#include <stdio.h>
#include <rune.h>

_BSD_RUNE_T_
___toupper(c)
	_BSD_RUNE_T_ c;
{
	int x;
	_RuneRange *rr = &_CurrentRuneLocale->mapupper_ext;
	_RuneEntry *re = rr->ranges;

	if (c == EOF)
		return(EOF);
	for (x = 0; x < rr->nranges; ++x, ++re) {
		if (c < re->min)
			return(c);
		if (c <= re->max)
			return(re->map + c - re->min);
	}
	return(c);
}

