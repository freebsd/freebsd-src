/*
 * tgoto.c
 *
 * By Ross Ridge
 * Public Domain
 * 92/02/01 07:30:33
 *
 * A few kludged attempts to worry outputing ^D's and NL's...
 * My advice is to get a decent terminal.
 *
 */

#include "defs.h"
#include <term.h>

#ifdef USE_SCCS_IDS
static const char SCCSid[] = "@(#) mytinfo tgoto.c 3.2 92/02/01 public domain, By Ross Ridge";
#endif

#ifdef USE_LITOUT_KLUDGE

/*
 * This kludge works by telling tputs to switch the tty driver to
 * "literal output" when printing the string so we don't have worry
 * about newlines and EOTs. The problem is that ioctls used to
 * switch modes might flush buffers and cause other problems.
 */


char *
tgoto(str, x, y)
char *str;
int x,y; {
	register char *sp;
	
	static char buf[MAX_LINE] = {'\\', '@'};

	sp = str = tparm(str, y, x);

	while (*sp != '\0') {
		if (*sp == '\004' || *sp == '\n') {
			strncpy(buf + 2, str, MAX_LINE - 2);
			buf[MAX_LINE - 2] = '\0';
			return buf;
		}
		sp++;
	}
	return sp;
}
#else

#ifdef USE_UPBC_KLUDGE

#ifdef USE_EXTERN_UPBC
extern char *BC, *UP;
#else
#define BC	cursor_left
#define UP	cursor_right
#endif

#ifdef __GNUC__
__inline__
#endif
static int
checkit(s)
register char *s; {
	while(*s != '\0') {
		if (*s == '\004' || *s == '\n')
			return 1;
		s++;
	}
	return 0;
}

/*
 * Major kludge, basically we just change the parmeters until we get
 * a string that doesn't contain a newline or EOT.
 */

char *
tgoto(str, x, y)
char *str;
int x,y; {
	static char buf[MAX_LINE];
	register char *orig, *s;
	int l;

	orig = tparm(str, y, x);

	if (!checkit(orig))
		return orig;

	s = tparm(str, y + 1, x);

	if (!checkit(s)) {
		if (BC == NULL)
			return s;
		l = strlen(s);
		strncpy(buf, s, MAX_LINE - 1);
		if (l < MAX_LINE - 1)
			strncpy(buf + l, BC, MAX_LINE - 1 - l);
		return s;
	}

	s = tparm(str, y, x + 1);

	if (!checkit(s)) {
		if (UP == NULL)
			return s;
		l = strlen(s);
		strncpy(buf, s, MAX_LINE - 1);
		if (l < MAX_LINE - 1)
			strncpy(buf + l, UP, MAX_LINE - 1 - l);
		return s;
	}

	s = tparm(str, y + 1, x + 1);

	if (!checkit(s)) {
		if (UP == NULL || BC == NULL)
			return s;
		l = strlen(s);
		strncpy(buf, s, MAX_LINE - 1);
		if (l < MAX_LINE - 1)
			strncpy(buf + l, UP, MAX_LINE - 1 - l);
		l += strlen(UP);
		if (l < MAX_LINE - 1)
			strncpy(buf + l, BC, MAX_LINE - 1 - l);
		return s;
	}

	return orig;
}

#else

/* the simple tgoto, don't worry about any of this newline/EOT crap */

char *
tgoto(str, x, y)
char *str;
int x,y; {
	return tparm(str, y, x);
}

#endif

#endif
