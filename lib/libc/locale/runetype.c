#include <stdio.h>
#include <rune.h>

unsigned long
___runetype(c)
	_BSD_RUNE_T_ c;
{
	int x;
	_RuneRange *rr = &_CurrentRuneLocale->runetype_ext;
	_RuneEntry *re = rr->ranges;

	if (c == EOF)
		return(0);
	for (x = 0; x < rr->nranges; ++x, ++re) {
		if (c < re->min)
			return(0L);
		if (c <= re->max) {
			if (re->types)
			    return(re->types[c - re->min]);
			else
			    return(re->map);
		}
	}
	return(0L);
}

