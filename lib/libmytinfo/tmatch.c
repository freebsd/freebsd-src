/*
 * tmatch.c
 *
 * By Ross Ridge
 * Public Domain
 * 92/02/01 07:30:35
 *
 * See if a terminal name matches a list of terminal names from a
 * terminal description
 *
 */

#include "defs.h"

#ifdef USE_SCCS_IDS
static const char SCCSid[] = "@(#) mytinfo tmatch.c 3.2 92/02/01 public domain, By Ross Ridge";
#endif

int
_tmatch(line, name)
char *line, *name; {
	char term[MAX_LINE];
	char *sp, *dp;

	sp = line;
	while (*sp != '\0') {
		dp = term;
		while (*sp != '\0' && *sp != '|')
			*dp++ = *sp++;
		*dp = '\0';
		if (strcmp(term, name) == 0)
			return 1;
		if (*sp == '|')
			sp++;
	}
	return 0;
}
