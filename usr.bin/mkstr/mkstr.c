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

#ifndef lint
static const char copyright[] =
"@(#) Copyright (c) 1980, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)mkstr.c	8.1 (Berkeley) 6/6/93";
#endif
#endif /* not lint */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define	ungetchar(c)	ungetc(c, stdin)

/*
 * mkstr - create a string error message file by massaging C source
 *
 * Bill Joy UCB August 1977
 *
 * Modified March 1978 to hash old messages to be able to recompile
 * without addding messages to the message file (usually)
 *
 * Based on an earlier program conceived by Bill Joy and Chuck Haley
 *
 * Program to create a string error message file
 * from a group of C programs.  Arguments are the name
 * of the file where the strings are to be placed, the
 * prefix of the new files where the processed source text
 * is to be placed, and the files to be processed.
 *
 * The program looks for 'error("' in the source stream.
 * Whenever it finds this, the following characters from the '"'
 * to a '"' are replaced by 'seekpt' where seekpt is a
 * pointer into the error message file.
 * If the '(' is not immediately followed by a '"' no change occurs.
 *
 * The optional '-' causes strings to be added at the end of the
 * existing error message file for recompilation of single routines.
 */

FILE	*mesgread, *mesgwrite;
char	name[100], *np;

void copystr(void);
int fgetNUL(char *, int, FILE *);
unsigned hashit(char *, char, unsigned);
void inithash(void);
int match(const char *);
int octdigit(char);
void process(void);
static void usage(void);

int
main(argc, argv)
	int argc;
	char *argv[];
{
	char addon = 0;

	argc--, argv++;
	if (argc > 1 && argv[0][0] == '-')
		addon++, argc--, argv++;
	if (argc < 3)
		usage();
	mesgwrite = fopen(argv[0], addon ? "a" : "w");
	if (mesgwrite == NULL)
		err(1, "%s", argv[0]);
	mesgread = fopen(argv[0], "r");
	if (mesgread == NULL)
		err(1, "%s", argv[0]);
	inithash();
	argc--, argv++;
	strcpy(name, argv[0]);
	np = name + strlen(name);
	argc--, argv++;
	do {
		strcpy(np, argv[0]);
		if (freopen(name, "w", stdout) == NULL)
			err(1, "%s", name);
		if (freopen(argv[0], "r", stdin) == NULL)
			err(1, "%s", argv[0]);
		process();
		argc--, argv++;
	} while (argc > 0);
	exit(0);
}

static void
usage()
{
	fprintf(stderr, "usage: mkstr [-] mesgfile prefix file ...\n");
	exit(1);
}

void
process()
{
	int c;

	for (;;) {
		c = getchar();
		if (c == EOF)
			return;
		if (c != 'e') {
			putchar(c);
			continue;
		}
		if (match("error(")) {
			printf("error(");
			c = getchar();
			if (c != '"')
				putchar(c);
			else
				copystr();
		}
	}
}

int
match(ocp)
	const char *ocp;
{
	const char *cp;
	int c;

	for (cp = ocp + 1; *cp; cp++) {
		c = getchar();
		if (c != *cp) {
			while (ocp < cp)
				putchar(*ocp++);
			ungetchar(c);
			return (0);
		}
	}
	return (1);
}

void
copystr()
{
	int c, ch;
	char buf[512];
	register char *cp = buf;

	for (;;) {
		c = getchar();
		if (c == EOF)
			break;
		switch (c) {

		case '"':
			*cp++ = 0;
			goto out;
		case '\\':
			c = getchar();
			switch (c) {

			case 'b':
				c = '\b';
				break;
			case 't':
				c = '\t';
				break;
			case 'r':
				c = '\r';
				break;
			case 'n':
				c = '\n';
				break;
			case '\n':
				continue;
			case 'f':
				c = '\f';
				break;
			case '0':
				c = 0;
				break;
			case '\\':
				break;
			default:
				if (!octdigit(c))
					break;
				c -= '0';
				ch = getchar();
				if (!octdigit(ch))
					break;
				c <<= 7, c += ch - '0';
				ch = getchar();
				if (!octdigit(ch))
					break;
				c <<= 3, c+= ch - '0', ch = -1;
				break;
			}
		}
		*cp++ = c;
	}
out:
	*cp = 0;
	printf("%d", hashit(buf, 1, NULL));
}

int
octdigit(c)
	char c;
{

	return (c >= '0' && c <= '7');
}

void
inithash()
{
	char buf[512];
	int mesgpt = 0;

	rewind(mesgread);
	while (fgetNUL(buf, sizeof buf, mesgread) != 0) {
		hashit(buf, 0, mesgpt);
		mesgpt += strlen(buf) + 2;
	}
}

#define	NBUCKETS	511

struct	hash {
	long	hval;
	unsigned hpt;
	struct	hash *hnext;
} *bucket[NBUCKETS];

unsigned
hashit(str, really, fakept)
	char *str;
	char really;
	unsigned fakept;
{
	int i;
	register struct hash *hp;
	char buf[512];
	long hashval = 0;
	register char *cp;

	if (really)
		fflush(mesgwrite);
	for (cp = str; *cp;)
		hashval = (hashval << 1) + *cp++;
	i = hashval % NBUCKETS;
	if (i < 0)
		i += NBUCKETS;
	if (really != 0)
		for (hp = bucket[i]; hp != 0; hp = hp->hnext)
		if (hp->hval == hashval) {
			fseek(mesgread, (long) hp->hpt, 0);
			fgetNUL(buf, sizeof buf, mesgread);
/*
			fprintf(stderr, "Got (from %d) %s\n", hp->hpt, buf);
*/
			if (strcmp(buf, str) == 0)
				break;
		}
	if (!really || hp == 0) {
		hp = (struct hash *) calloc(1, sizeof *hp);
		if (hp == NULL)
			err(1, NULL);
		hp->hnext = bucket[i];
		hp->hval = hashval;
		hp->hpt = really ? ftell(mesgwrite) : fakept;
		if (really) {
			fwrite(str, sizeof (char), strlen(str) + 1, mesgwrite);
			fwrite("\n", sizeof (char), 1, mesgwrite);
		}
		bucket[i] = hp;
	}
/*
	fprintf(stderr, "%s hashed to %ld at %d\n", str, hp->hval, hp->hpt);
*/
	return (hp->hpt);
}

#include <sys/types.h>
#include <sys/stat.h>

int
fgetNUL(obuf, rmdr, file)
	char *obuf;
	register int rmdr;
	FILE *file;
{
	int c;
	register char *buf = obuf;

	while (--rmdr > 0 && (c = getc(file)) != 0 && c != EOF)
		*buf++ = c;
	*buf++ = 0;
	getc(file);
	return ((feof(file) || ferror(file)) ? 0 : 1);
}
