/*
 * findcap.c
 *
 * By Ross Ridge
 * Public Domain
 * 92/02/01 07:29:55
 *
 */

#include "defs.h"
#include <term.h>

#include "bsearch.c"

#ifdef USE_SCCS_IDS
static const char SCCSid[] = "@(#) mytinfo findcap.c 3.2 92/02/01 public domain, By Ross Ridge";
#endif

extern char **_sboolcodes[], **_snumcodes[], **_sstrcodes[];
extern char **_sboolnames[], **_snumnames[], **_sstrnames[];
extern char **_sboolfnames[], **_snumfnames[], **_sstrfnames[];

static char **p2p2c;

int
_findboolcode(s)
char *s; {
	char ***match;

	p2p2c = &s;

	match = (char ***) bsearch((anyptr) &p2p2c, (anyptr) _sboolcodes,
				   NUM_OF_BOOLS, sizeof(p2p2c), _compar);
	if (match == NULL)
		return -1;
	return *match - boolcodes;
}

int
_findboolname(s)
char *s; {
	char ***match;

	p2p2c = &s;

	match = (char ***) bsearch((anyptr) &p2p2c, (anyptr) _sboolnames,
				   NUM_OF_BOOLS, sizeof(p2p2c), _compar);
	if (match == NULL)
		return -1;
	return *match - boolnames;
}

int
_findboolfname(s)
char *s; {
	char ***match;

	p2p2c = &s;

	match = (char ***) bsearch((anyptr) &p2p2c, (anyptr) _sboolfnames,
				   NUM_OF_BOOLS, sizeof(p2p2c), _compar);
	if (match == NULL)
		return -1;
	return *match - boolfnames;
}

int
_findnumcode(s)
char *s; {
	char ***match;

	p2p2c = &s;

	match = (char ***) bsearch((anyptr) &p2p2c, (anyptr) _snumcodes,
				   NUM_OF_NUMS, sizeof(p2p2c), _compar);
	if (match == NULL)
		return -1;
	return *match - numcodes;
}

int
_findnumname(s)
char *s; {
	char ***match;

	p2p2c = &s;

	match = (char ***) bsearch((anyptr) &p2p2c, (anyptr) _snumnames,
				   NUM_OF_NUMS, sizeof(p2p2c), _compar);
	if (match == NULL)
		return -1;
	return *match - numnames;
}

int
_findnumfname(s)
char *s; {
	char ***match;

	p2p2c = &s;

	match = (char ***) bsearch((anyptr) &p2p2c, (anyptr) _snumfnames,
				   NUM_OF_NUMS, sizeof(p2p2c), _compar);
	if (match == NULL)
		return -1;
	return *match - numfnames;
}

int
_findstrcode(s)
char *s; {
	char ***match;

	p2p2c = &s;

	match = (char ***) bsearch((anyptr) &p2p2c, (anyptr) _sstrcodes,
				   NUM_OF_STRS, sizeof(p2p2c), _compar);
	if (match == NULL)
		return -1;
	return *match - strcodes;
}

int
_findstrname(s)
char *s; {
	char ***match;

	p2p2c = &s;

	match = (char ***) bsearch((anyptr) &p2p2c, (anyptr) _sstrnames,
				   NUM_OF_STRS, sizeof(p2p2c), _compar);
	if (match == NULL)
		return -1;
	return *match - strnames;
}

int
_findstrfname(s)
char *s; {
	char ***match;

	p2p2c = &s;

	match = (char ***) bsearch((anyptr) &p2p2c, (anyptr) _sstrfnames,
				   NUM_OF_STRS, sizeof(p2p2c), _compar);
	if (match == NULL)
		return -1;
	return *match - strfnames;
}
