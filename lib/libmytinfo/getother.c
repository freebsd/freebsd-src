/*
 * getother.c
 *
 * By Ross Ridge
 * Public Domain
 * 92/02/01 07:29:58
 *
 */

#include "defs.h"
#include <term.h>

#ifdef USE_SCCS_IDS
static const char SCCSid[] = "@(#) mytinfo getother.c 3.2 92/02/01 public domain, By Ross Ridge";
#endif

int
_getother(name, path, ct)
char *name;
struct term_path *path;
TERMINAL *ct; {
	static int depth = 0;
	int r;
	char buf[MAX_BUF];

	if (depth >= MAX_DEPTH)
		return 1;		/* infinite loop */

#ifdef DEBUG
	printf("\ngetother: %s\n", name);
#endif

	switch(_findterm(name, path, buf)) {
	case -3:
		return 1;
	case 1:
		depth++;
		r = _gettcap(buf, ct, path);
		break;
	case 2:
		depth++;
		r = _gettinfo(buf, ct, path);
		break;
	default:
		return 0;
	}
	depth--;
	return r;
}
