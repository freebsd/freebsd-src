#include <stdio.h>
#include <ctype.h>
#include <rune.h>

#if !defined(_USE_CTYPE_INLINE_) && !defined(_USE_CTYPE_MACROS_)
/*
 * See comments in <machine/ansi.h>
 */
int
__istype(c, f)
	_BSD_RUNE_T_ c;
	unsigned long f;
{
	if (c == EOF)
		return 0;
	if (c < 0)
		c = (unsigned char) c;
	return ((((c & _CRMASK) ? ___runetype(c)
           : _CurrentRuneLocale->runetype[c]) & f) ? 1 : 0);
}

int
__isctype(_BSD_RUNE_T_ c, unsigned long f)
	_BSD_RUNE_T_ c;
	unsigned long f;
{
	if (c == EOF)
		return 0;
	if (c < 0)
		c = (unsigned char) c;
	return ((((c & _CRMASK) ? 0
           : _DefaultRuneLocale.runetype[c]) & f) ? 1 : 0);
}

_BSD_RUNE_T_
toupper(c)
	_BSD_RUNE_T_ c;
{
	if (c == EOF)
		return EOF;
	if (c < 0)
		c = (unsigned char) c;
	return ((c & _CRMASK) ?
	    ___toupper(c) : _CurrentRuneLocale->mapupper[c]);
}

_BSD_RUNE_T_
tolower(c)
	_BSD_RUNE_T_ c;
{
	if (c == EOF)
		return EOF;
	if (c < 0)
		c = (unsigned char) c;
	return ((c & _CRMASK) ?
	    ___tolower(c) : _CurrentRuneLocale->maplower[c]);
}
#endif
