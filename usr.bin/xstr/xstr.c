/*
 * Copyright (c) 1980, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>

__FBSDID("$FreeBSD$");

#ifndef lint
static const char copyright[] =
"@(#) Copyright (c) 1980, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif

#ifndef lint
static const char sccsid[] = "@(#)xstr.c	8.1 (Berkeley) 6/9/93";
#endif

#include <sys/types.h>

#include <ctype.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

#include "pathnames.h"

/*
 * xstr - extract and hash strings in a C program
 *
 * Bill Joy UCB
 * November, 1978
 */

#define	ignore(a)	((void) a)

off_t	tellpt;

off_t	mesgpt;
char	cstrings[] =	"strings";
char	*strings =	cstrings;

int	cflg;
int	vflg;
int	readstd;

char lastchr(char *);

int fgetNUL(char *, int, FILE *);
int istail(char *, char *);
int octdigit(char);
int xgetc(FILE *);

off_t hashit(char *, int);
off_t yankstr(char **);

static void usage(void);

void flushsh(void);
void found(int, off_t, char *);
void inithash(void);
void onintr(int);
void process(const char *);
void prstr(char *);
void xsdotc(void);

int
main(argc, argv)
	int argc;
	char *argv[];
{
	int c;

	while ((c = getopt(argc, argv, "-cv")) != -1)
		switch (c) {
		case '-':
			readstd++;
			break;
		case 'c':
			cflg++;
			break;
		case 'v':
			vflg++;
			break;
		default:
			usage();
		}
	argc -= optind;
	argv += optind;
		
	if (signal(SIGINT, SIG_IGN) == SIG_DFL)
		signal(SIGINT, onintr);
	if (cflg || (argc == 0 && !readstd))
		inithash();
	else
		strings = mktemp(strdup(_PATH_TMP));
	while (readstd || argc > 0) {
		if (freopen("x.c", "w", stdout) == NULL)
			err(1, "x.c");
		if (!readstd && freopen(argv[0], "r", stdin) == NULL)
			err(2, "%s", argv[0]);
		process("x.c");
		if (readstd == 0)
			argc--, argv++;
		else
			readstd = 0;
	};
	flushsh();
	if (cflg == 0)
		xsdotc();
	if (strings[0] == '/')
		ignore(unlink(strings));
	exit(0);
}

static void
usage()
{
	fprintf(stderr, "usage: xstr [-v] [-c] [-] [name ...]\n");
	exit (1);
}

char linebuf[BUFSIZ];

void
process(name)
	const char *name;
{
	char *cp;
	int c;
	int incomm = 0;
	int ret;

	printf("extern char\txstr[];\n");
	for (;;) {
		if (fgets(linebuf, sizeof linebuf, stdin) == NULL) {
			if (ferror(stdin))
				err(3, "%s", name);
			break;
		}
		if (linebuf[0] == '#') {
			if (linebuf[1] == ' ' && isdigit(linebuf[2]))
				printf("#line%s", &linebuf[1]);
			else
				printf("%s", linebuf);
			continue;
		}
		for (cp = linebuf; (c = *cp++);) switch (c) {

		case '"':
			if (incomm)
				goto def;
			if ((ret = (int) yankstr(&cp)) == -1)
				goto out;
			printf("(&xstr[%d])", ret);
			break;

		case '\'':
			if (incomm)
				goto def;
			putchar(c);
			if (*cp)
				putchar(*cp++);
			break;

		case '/':
			if (incomm || *cp != '*')
				goto def;
			incomm = 1;
			cp++;
			printf("/*");
			continue;

		case '*':
			if (incomm && *cp == '/') {
				incomm = 0;
				cp++;
				printf("*/");
				continue;
			}
			goto def;

def:
		default:
			putchar(c);
			break;
		}
	}
out:
	if (ferror(stdout))
		warn("x.c"), onintr(0);
}

off_t
yankstr(cpp)
	char **cpp;
{
	char *cp = *cpp;
	int c, ch;
	char dbuf[BUFSIZ];
	char *dp = dbuf;
	char *tp;
	static char tmp[] = "b\bt\tr\rn\nf\f\\\\\"\"";

	while ((c = *cp++)) {
		switch (c) {

		case '"':
			cp++;
			goto out;

		case '\\':
			c = *cp++;
			if (c == 0)
				break;
			if (c == '\n') {
				if (fgets(linebuf, sizeof linebuf, stdin)
				    == NULL) {
					if (ferror(stdin))
						err(3, "x.c");
					return(-1);
				}
				cp = linebuf;
				continue;
			}
			for (tp = tmp; (ch = *tp++); tp++)
				if (c == ch) {
					c = *tp;
					goto gotc;
				}
			if (!octdigit(c)) {
				*dp++ = '\\';
				break;
			}
			c -= '0';
			if (!octdigit(*cp))
				break;
			c <<= 3, c += *cp++ - '0';
			if (!octdigit(*cp))
				break;
			c <<= 3, c += *cp++ - '0';
			break;
		}
gotc:
		*dp++ = c;
	}
out:
	*cpp = --cp;
	*dp = 0;
	return (hashit(dbuf, 1));
}

int
octdigit(c)
	char c;
{
	return (isdigit(c) && c != '8' && c != '9');
}

void
inithash()
{
	char buf[BUFSIZ];
	FILE *mesgread = fopen(strings, "r");

	if (mesgread == NULL)
		return;
	for (;;) {
		mesgpt = tellpt;
		if (fgetNUL(buf, sizeof buf, mesgread) == 0)
			break;
		ignore(hashit(buf, 0));
	}
	ignore(fclose(mesgread));
}

int
fgetNUL(obuf, rmdr, file)
	char *obuf;
	int rmdr;
	FILE *file;
{
	int c;
	char *buf = obuf;

	while (--rmdr > 0 && (c = xgetc(file)) != 0 && c != EOF)
		*buf++ = c;
	*buf++ = 0;
	return ((feof(file) || ferror(file)) ? 0 : 1);
}

int
xgetc(file)
	FILE *file;
{

	tellpt++;
	return (getc(file));
}

#define	BUCKETS	128

struct	hash {
	off_t	hpt;
	char	*hstr;
	struct	hash *hnext;
	short	hnew;
} bucket[BUCKETS];

off_t
hashit(str, new)
	char *str;
	int new;
{
	int i;
	struct hash *hp, *hp0;

	hp = hp0 = &bucket[lastchr(str) & 0177];
	while (hp->hnext) {
		hp = hp->hnext;
		i = istail(str, hp->hstr);
		if (i >= 0)
			return (hp->hpt + i);
	}
	if ((hp = (struct hash *) calloc(1, sizeof (*hp))) == NULL)
		errx(8, "calloc");
	hp->hpt = mesgpt;
	if (!(hp->hstr = strdup(str)))
		err(1, NULL);
	mesgpt += strlen(hp->hstr) + 1;
	hp->hnext = hp0->hnext;
	hp->hnew = new;
	hp0->hnext = hp;
	return (hp->hpt);
}

void
flushsh()
{
	int i;
	struct hash *hp;
	FILE *mesgwrit;
	int old = 0, new = 0;

	for (i = 0; i < BUCKETS; i++)
		for (hp = bucket[i].hnext; hp != NULL; hp = hp->hnext)
			if (hp->hnew)
				new++;
			else
				old++;
	if (new == 0 && old != 0)
		return;
	mesgwrit = fopen(strings, old ? "r+" : "w");
	if (mesgwrit == NULL)
		perror(strings), exit(4);
	for (i = 0; i < BUCKETS; i++)
		for (hp = bucket[i].hnext; hp != NULL; hp = hp->hnext) {
			found(hp->hnew, hp->hpt, hp->hstr);
			if (hp->hnew) {
				fseek(mesgwrit, hp->hpt, 0);
				ignore(fwrite(hp->hstr, strlen(hp->hstr) + 1, 1, mesgwrit));
				if (ferror(mesgwrit))
					err(4, "%s", strings);
			}
		}
	if (fclose(mesgwrit) == EOF)
		err(4, "%s", strings);
}

void
found(new, off, str)
	int new;
	off_t off;
	char *str;
{
	if (vflg == 0)
		return;
	if (!new)
		fprintf(stderr, "found at %d:", (int) off);
	else
		fprintf(stderr, "new at %d:", (int) off);
	prstr(str);
	fprintf(stderr, "\n");
}

void
prstr(cp)
	char *cp;
{
	int c;

	while ((c = (*cp++ & 0377)))
		if (c < ' ')
			fprintf(stderr, "^%c", c + '`');
		else if (c == 0177)
			fprintf(stderr, "^?");
		else if (c > 0200)
			fprintf(stderr, "\\%03o", c);
		else
			fprintf(stderr, "%c", c);
}

void
xsdotc()
{
	FILE *strf = fopen(strings, "r");
	FILE *xdotcf;

	if (strf == NULL)
		err(5, "%s", strings);
	xdotcf = fopen("xs.c", "w");
	if (xdotcf == NULL)
		err(6, "xs.c");
	fprintf(xdotcf, "char\txstr[] = {\n");
	for (;;) {
		int i, c;

		for (i = 0; i < 8; i++) {
			c = getc(strf);
			if (ferror(strf)) {
				warn("%s", strings);
				onintr(0);
			}
			if (feof(strf)) {
				fprintf(xdotcf, "\n");
				goto out;
			}
			fprintf(xdotcf, "0x%02x,", c);
		}
		fprintf(xdotcf, "\n");
	}
out:
	fprintf(xdotcf, "};\n");
	ignore(fclose(xdotcf));
	ignore(fclose(strf));
}

char
lastchr(cp)
	char *cp;
{

	while (cp[0] && cp[1])
		cp++;
	return (*cp);
}

int
istail(str, of)
	char *str, *of;
{
	int d = strlen(of) - strlen(str);

	if (d < 0 || strcmp(&of[d], str) != 0)
		return (-1);
	return (d);
}

void
onintr(dummy)
	int dummy __unused;
{

	ignore(signal(SIGINT, SIG_IGN));
	if (strings[0] == '/')
		ignore(unlink(strings));
	ignore(unlink("x.c"));
	ignore(unlink("xs.c"));
	exit(7);
}
