/*
 * tiget.c
 *
 * By Ross Ridge
 * Public Domain
 * 92/02/01 07:30:34
 *
 * The various tiget terminfo functions.
 */

#include "defs.h"
#include <term.h>

#ifdef USE_SCCS_IDS
static const char SCCSid[] = "@(#) mytinfo tiget.c 3.2 92/02/01 public domain, By Ross Ridge";
#endif

int
tigetnum(cap)
char *cap; {
	int ind;

	ind = _findnumname(cap);
	if (ind == -1)
		return -2;
	return cur_term->nums[ind];
}

int
tigetflag(cap)
char *cap; {
	int ind;

	ind = _findboolname(cap);
	if (ind == -1)
		return -1;
	return cur_term->bools[ind];
}

char *
tigetstr(cap)
char *cap; {
	int ind;

	ind = _findstrname(cap);
	if (ind == -1)
		return (char *) -1;
	return cur_term->strs[ind];
}


