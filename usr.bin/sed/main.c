/*-
 * Copyright (c) 1992 Diomidis Spinellis.
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Diomidis Spinellis of Imperial College, University of London.
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
"@(#) Copyright (c) 1992, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)main.c	8.2 (Berkeley) 1/3/94";
#endif
static const char rcsid[] =
	"$Id: main.c,v 1.3.2.2 1997/08/12 06:36:12 charnier Exp $";
#endif /* not lint */

#include <sys/types.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <locale.h>
#include <regex.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "defs.h"
#include "extern.h"

/*
 * Linked list of units (strings and files) to be compiled
 */
struct s_compunit {
	struct s_compunit *next;
	enum e_cut {CU_FILE, CU_STRING} type;
	char *s;			/* Pointer to string or fname */
};

/*
 * Linked list pointer to compilation units and pointer to current
 * next pointer.
 */
static struct s_compunit *script, **cu_nextp = &script;

/*
 * Linked list of files to be processed
 */
struct s_flist {
	char *fname;
	struct s_flist *next;
};

/*
 * Linked list pointer to files and pointer to current
 * next pointer.
 */
static struct s_flist *files, **fl_nextp = &files;

int aflag, eflag, nflag;

/*
 * Current file and line number; line numbers restart across compilation
 * units, but span across input files.
 */
char *fname;			/* File name. */
u_long linenum;
int lastline;			/* TRUE on the last line of the last file */

static void add_compunit __P((enum e_cut, char *));
static void add_file __P((char *));
static void usage __P((void));

int
main(argc, argv)
	int argc;
	char *argv[];
{
	int c, fflag;

	(void) setlocale(LC_ALL, "");

	fflag = 0;
	while ((c = getopt(argc, argv, "ae:f:n")) !=  -1)
		switch (c) {
		case 'a':
			aflag = 1;
			break;
		case 'e':
			eflag = 1;
			add_compunit(CU_STRING, optarg);
			break;
		case 'f':
			fflag = 1;
			add_compunit(CU_FILE, optarg);
			break;
		case 'n':
			nflag = 1;
			break;
		default:
		case '?':
			usage();
		}
	argc -= optind;
	argv += optind;

	/* First usage case; script is the first arg */
	if (!eflag && !fflag && *argv) {
		add_compunit(CU_STRING, *argv);
		argv++;
	}

	compile();

	/* Continue with first and start second usage */
	if (*argv)
		for (; *argv; argv++)
			add_file(*argv);
	else
		add_file(NULL);
	process();
	cfclose(prog, NULL);
	if (fclose(stdout))
		err(1, "stdout");
	exit (0);
}

static void
usage()
{
	(void)fprintf(stderr, "%s\n%s\n",
		"usage: sed script [-an] [file ...]",
		"       sed [-an] [-e script] ... [-f script_file] ... [file ...]");
	exit(1);
}

/*
 * Like fgets, but go through the chain of compilation units chaining them
 * together.  Empty strings and files are ignored.
 */
char *
cu_fgets(buf, n)
	char *buf;
	int n;
{
	static enum {ST_EOF, ST_FILE, ST_STRING} state = ST_EOF;
	static FILE *f;		/* Current open file */
	static char *s;		/* Current pointer inside string */
	static char string_ident[30];
	char *p;

again:
	switch (state) {
	case ST_EOF:
		if (script == NULL)
			return (NULL);
		linenum = 0;
		switch (script->type) {
		case CU_FILE:
			if ((f = fopen(script->s, "r")) == NULL)
				err(1, "%s", script->s);
			fname = script->s;
			state = ST_FILE;
			goto again;
		case CU_STRING:
			if ((snprintf(string_ident,
			    sizeof(string_ident), "\"%s\"", script->s)) >=
			    sizeof(string_ident) - 1)
				(void)strcpy(string_ident +
				    sizeof(string_ident) - 6, " ...\"");
			fname = string_ident;
			s = script->s;
			state = ST_STRING;
			goto again;
		}
	case ST_FILE:
		if ((p = fgets(buf, n, f)) != NULL) {
			linenum++;
			if (linenum == 1 && buf[0] == '#' && buf[1] == 'n')
				nflag = 1;
			return (p);
		}
		script = script->next;
		(void)fclose(f);
		state = ST_EOF;
		goto again;
	case ST_STRING:
		if (linenum == 0 && s[0] == '#' && s[1] == 'n')
			nflag = 1;
		p = buf;
		for (;;) {
			if (n-- <= 1) {
				*p = '\0';
				linenum++;
				return (buf);
			}
			switch (*s) {
			case '\0':
				state = ST_EOF;
				if (s == script->s) {
					script = script->next;
					goto again;
				} else {
					script = script->next;
					*p = '\0';
					linenum++;
					return (buf);
				}
			case '\n':
				*p++ = '\n';
				*p = '\0';
				s++;
				linenum++;
				return (buf);
			default:
				*p++ = *s++;
			}
		}
	}
	/* NOTREACHED */
	return (NULL);
}

/*
 * Like fgets, but go through the list of files chaining them together.
 * Set len to the length of the line.
 */
int
mf_fgets(sp, spflag)
	SPACE *sp;
	enum e_spflag spflag;
{
	static FILE *f;		/* Current open file */
	size_t len;
	char *p;
	int c;

	if (f == NULL)
		/* Advance to first non-empty file */
		for (;;) {
			if (files == NULL) {
				lastline = 1;
				return (0);
			}
			if (files->fname == NULL) {
				f = stdin;
				fname = "stdin";
			} else {
				fname = files->fname;
				if ((f = fopen(fname, "r")) == NULL)
					err(1, "%s", fname);
			}
			if ((c = getc(f)) != EOF) {
				(void)ungetc(c, f);
				break;
			}
			(void)fclose(f);
			files = files->next;
		}

	if (lastline) {
		sp->len = 0;
		return (0);
	}

	/*
	 * Use fgetln so that we can handle essentially infinite input data.
	 * Can't use the pointer into the stdio buffer as the process space
	 * because the ungetc() can cause it to move.
	 */
	p = fgetln(f, &len);
	if (ferror(f))
		errx(1, "%s: %s", fname, strerror(errno ? errno : EIO));
	cspace(sp, p, len, spflag);

	linenum++;
	/* Advance to next non-empty file */
	while ((c = getc(f)) == EOF) {
		(void)fclose(f);
		files = files->next;
		if (files == NULL) {
			lastline = 1;
			return (1);
		}
		if (files->fname == NULL) {
			f = stdin;
			fname = "stdin";
		} else {
			fname = files->fname;
			if ((f = fopen(fname, "r")) == NULL)
				err(1, "%s", fname);
		}
	}
	(void)ungetc(c, f);
	return (1);
}

/*
 * Add a compilation unit to the linked list
 */
static void
add_compunit(type, s)
	enum e_cut type;
	char *s;
{
	struct s_compunit *cu;

	cu = xmalloc(sizeof(struct s_compunit));
	cu->type = type;
	cu->s = s;
	cu->next = NULL;
	*cu_nextp = cu;
	cu_nextp = &cu->next;
}

/*
 * Add a file to the linked list
 */
static void
add_file(s)
	char *s;
{
	struct s_flist *fp;

	fp = xmalloc(sizeof(struct s_flist));
	fp->next = NULL;
	*fl_nextp = fp;
	fp->fname = s;
	fl_nextp = &fp->next;
}
