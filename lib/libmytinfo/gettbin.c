/*
 * gettbin.c
 *
 * By Ross Ridge
 * Public Domain
 * 92/02/01 07:29:59
 *
 * Get a terminfo binary entry
 *
 */

#include "defs.h"
#include <term.h>

#ifdef USE_SCCS_IDS
static const char SCCSid[] = "@(#) mytinfo gettbin.c 3.2 92/02/01 public domain, By Ross Ridge";
#endif

extern int _boolorder[], _numorder[], _strorder[];

#ifdef TRUE_BYTE_ORDERING
/* 8 bit char, 16 bit short, lsb first, twos complement */
#define convshort(s) (*(short *)(s))

#else

#ifdef TWOS_COPLEMENT
/* 8 bit char, 16 bit short, lsb last, twos complement */
#define convshort(s) ((short)(((s[0] & 0377) << 8) | (s[1] & 0377)))

#else

/* anything else... */

static short
convshort(s)
char *s; {
	register int a,b;

	a = (int) s[0] & 0377;
	b = (int) s[1] & 0377;

	if (a == 0377 && b == 0377)
		return -1;
	if (a == 0376 && b == 0377)
		return -2;

	return a + b * 256;
}
#endif
#endif

int
_gettbin(buf, cur)
char *buf;
TERMINAL *cur; {
	register char *s;
	int i;
	int sz_names, sz_bools, sz_nums, sz_offs, sz_strs;
	int n_bools, n_nums, n_strs;
	char *strtbl;

	buf[MAX_BUF-1] = '\0';
	s = buf;

	if (convshort(s) != 0432)
		return 1;
	sz_names = convshort(s + 2);
	sz_bools = convshort(s + 4);
	n_nums = convshort(s + 6);
	n_strs = convshort(s + 8);
	sz_strs = convshort(s + 10);

	n_bools = sz_bools;
	sz_nums = n_nums * 2;
	sz_offs = n_strs * 2;

	if ((sz_names + sz_bools) & 1)
		sz_bools++;

	if (12 + sz_names + sz_bools + sz_nums + sz_offs + sz_strs >= MAX_BUF)
		return 1;

	s += 12;
	if ((cur->name_all = _addstr(s)) == NULL)
		return 1;
	s += sz_names;
	while(--s >= buf + 12) {
		if (*s == '|') {
			if ((cur->name_long = _addstr(s + 1)) == NULL)
				return 1;
			break;
		}
	}

	s = buf + 12 + sz_names;
	for(i = 0; i < n_bools && _boolorder[i] != -1; i++, s++) {
		if (cur->bools[_boolorder[i]] == -1 && *s == 1)
			cur->bools[_boolorder[i]] = 1;
	}

	s = buf + 12 + sz_names + sz_bools;
	for(i = 0; i < n_nums && _numorder[i] != -1; i++, s += 2) {
		if (convshort(s) == -2)
			cur->nums[_numorder[i]] = -1;
		else if (cur->nums[_numorder[i]] == -2 && convshort(s) != -1)
			cur->nums[_numorder[i]] = convshort(s);
	}

	s = buf + 12 + sz_names + sz_bools + sz_nums;
	strtbl = s + sz_offs;
	for(i = 0; i < n_strs && _strorder[i] != -1; i++, s += 2) {
		if (convshort(s) == -2)
			cur->strs[_strorder[i]] = NULL;
		else if (cur->strs[_strorder[i]] == (char *) -1
			 && convshort(s) != -1) {
#ifdef DEBUG
			printf("$%s ", strnames[_strorder[i]]);
#endif
			if ((cur->strs[_strorder[i]]
			     = _addstr(strtbl + convshort(s))) == NULL)
				return 1;
		}
	}

	return 0;
}
