/*
 * readcaps.c
 *
 * By Ross Ridge
 * Public Domain
 * 92/02/01 07:30:15
 *
 * Read in the cap_list file
 *
 */

#define NOTLIB
#include "defs.h"

#include <ctype.h>

#ifdef USE_SCCS_IDS
static const char SCCSid[] = "@(#) mytinfo readcaps.c 3.2 92/02/01 public domain, By Ross Ridge";
#endif

#ifdef __GNUC__
__inline__
#endif
static int
skipline(f)
register FILE *f; {
	register int c;

	do {
		c = getc(f);
		if (c == EOF)
			return EOF;
#ifdef TEST
		putchar(c);
#endif
	} while (c != '\n');

	return 0;
}

#ifdef __GNUC__
__inline__
#endif
static int
getfield(f, s, len)
register FILE *f;
register char *s;
int len; {
	register int c;
	int i;
#ifdef TEST
	char *start = s;
#endif

	do {
		c = getc(f);
		if (c == EOF)
			return EOF;
	} while (c != '\n' && isspace(c));
	if (c == '\n')
		return 0;

	i = 0;
	while(!isspace(c)) {
		if (i++ < len)
			*s++ = c;
		c = getc(f);
		if (c == EOF)
			return EOF;

	}
	*s = '\0';
#ifdef TEST
	printf(" %s", start);
#endif
	return c;
}

int
readcaps(f, buf, max)
FILE *f;
register struct caplist *buf;
int max; {
	int type;
	register int count;
	int c;
	static char dummy;

	count = 0;
	type = getc(f);
	while(type != EOF) {
		if (type == '$' || type == '!' || type == '#') {
			if (count >= max)
				return count + 1;
#ifdef TEST
			putchar(type);
#endif
			buf[count].type = type;

			if (type == '$') {
				c = getc(f);
				if (c == EOF)
					break;
				if (c == 'G')
					buf[count].flag = 'G';
				else if (c == 'K')
					buf[count].flag = 'K';
				else
					buf[count].flag = ' ';
			}

			c = getfield(f, buf[count].var, MAX_VARNAME);
			if (c == EOF || c == '\n' || c == 0)
				return -1;
			c = getfield(f, buf[count].tinfo, MAX_TINFONAME);
			if (c == EOF || c == '\n' || c == 0)
				return -1;
			c = getfield(f, buf[count].tcap, MAX_TCAPNAME);
			if (c == EOF || c == 0)
				return -1;
			if (c != '\n')
				if (getfield(f, &dummy, 1) != 0)
					return -1;
			count++;
#ifdef TEST
			putchar('\n');
#endif
		} else {
#ifdef TEST
			putchar(type);
#endif
			if (type != '\n' && skipline(f) == EOF)
				return -1;
		}
		type = getc(f);
	}
	return count;
}

#ifdef TEST
struct caplist list[1000];

int
main() {
	int ret;

	ret = readcaps(stdin, list, 1000);
	printf("ret = %d\n", ret);
	return 0;
}
#endif
