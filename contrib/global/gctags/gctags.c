/*
 * Copyright (c) 1998 Shigio Yamaguchi. All rights reserved.
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
 *      This product includes software developed by Shigio Yamaguchi.
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
 *
 *	gctags.c				13-Sep-98
 */

#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "global.h"
#include "gctags.h"

int	bflag;			/* -b: force level 1 block start */
int	dflag;			/* -d: debug */
int	eflag;			/* -e: force level 1 block end */
int	nflag;			/* -n: doen't print tag */
int	rflag;			/* -r: function reference */
int	sflag;			/* -s: collect symbols */
int	wflag;			/* -w: warning message */
int	yaccfile;		/* yacc file */

const char *progname = "gctags";	/* program name */
char	*notfunction;

int	main __P((int, char **));
static	void usage __P((void));

struct words *words;
static int tablesize;

int
main(argc, argv)
	int	argc;
	char	**argv;
{
	char	*p;

	while (--argc > 0 && (++argv)[0][0] == '-') {
		for (p = argv[0] + 1; *p; p++) {
			switch(*p) {
			case 'b':
				bflag++;
				break;
			case 'd':
				dflag++;
				break;
			case 'e':
				eflag++;
				break;
			case 'n':
				nflag++;
				break;
			case 'r':
				rflag++;
				sflag = 0;
				break;
			case 's':
				sflag++;
				rflag = 0;
				break;
			case 'w':
				wflag++;
				break;
			default:
				usage();
			}
		}
	}
	if (argc < 1)
		usage();
	if (getenv("GTAGSWARNING"))
		wflag++;	
	if (test("r", NOTFUNCTION)) {
		FILE	*ip;
		STRBUF	*sb = stropen();
		int	i;

		if ((ip = fopen(NOTFUNCTION, "r")) == 0)
			die1("'%s' cannot read.", NOTFUNCTION);
		for (tablesize = 0; (p = mgets(ip, NULL, 0)) != NULL; tablesize++)
			strnputs(sb, p, strlen(p) + 1);
		fclose(ip);
		if ((words = malloc(sizeof(struct words) * tablesize)) == NULL)
			die("short of memory.");
		p = strvalue(sb);
		for (i = 0; i < tablesize; i++) {
			words[i].name = p;
			p += strlen(p) + 1;
		}
		qsort(words, tablesize, sizeof(struct words), cmp);
		/* don't call strclose(sb); */
	}
	for (; argc > 0; argv++, argc--) {
		if (!opentoken(argv[0]))
			die1("'%s' cannot open.", argv[0]);
		if (locatestring(argv[0], ".y", MATCH_AT_LAST))
			C(1);
		else if (locatestring(argv[0], ".s", MATCH_AT_LAST) ||
			locatestring(argv[0], ".S", MATCH_AT_LAST))
			assembler();
		else if (locatestring(argv[0], ".java", MATCH_AT_LAST))
			java();
		else
			C(0);
		closetoken();
	}
	exit(0);
}

static void
usage()
{
	(void)fprintf(stderr, "usage: gctags [-benrsw] file ...\n");
	exit(1);
}

int
cmp(s1, s2)
	const void *s1, *s2;
{
	return strcmp(((struct words *)s1)->name, ((struct words *)s2)->name);
}

int
isnotfunction(name)
	char	*name;
{
	struct words tmp;
	struct words *result;

	if (words == NULL)
		return 0;
	tmp.name = name;
	result = (struct words *)bsearch(&tmp, words, tablesize, sizeof(struct words), cmp);
	return (result != NULL) ? 1 : 0;
}

#ifdef DEBUG
void
dbg_print(level, s)
	int level;
	const char *s;
{
	if (!dflag)
		return;
	fprintf(stderr, "[%04d]", lineno);
	for (; level > 0; level--)
		fprintf(stderr, "    ");
	fprintf(stderr, "%s\n", s);
}
#endif
