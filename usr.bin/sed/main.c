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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#ifndef lint
static const char copyright[] =
"@(#) Copyright (c) 1992, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif

#ifndef lint
static const char sccsid[] = "@(#)main.c	8.2 (Berkeley) 1/3/94";
#endif

#include <sys/param.h>
#include <sys/stat.h>

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
int rflags = 0;

/*
 * Current file and line number; line numbers restart across compilation
 * units, but span across input files.
 */
const char *fname;		/* File name. */
const char *inplace;		/* Inplace edit file extension. */
u_long linenum;
int lastline;			/* TRUE on the last line of the last file */

static void add_compunit(enum e_cut, char *);
static void add_file(char *);
static int inplace_edit(char **);
static void usage(void);

int
main(argc, argv)
	int argc;
	char *argv[];
{
	int c, fflag;
	char *temp_arg;

	(void) setlocale(LC_ALL, "");

	fflag = 0;
	inplace = NULL;

	while ((c = getopt(argc, argv, "Eae:f:i:n")) != -1)
		switch (c) {
		case 'E':
			rflags = REG_EXTENDED;
			break;
		case 'a':
			aflag = 1;
			break;
		case 'e':
			eflag = 1;
			if ((temp_arg = malloc(strlen(optarg) + 2)) == NULL)
				err(1, "malloc");
			strcpy(temp_arg, optarg);
			strcat(temp_arg, "\n");
			add_compunit(CU_STRING, temp_arg);
			break;
		case 'f':
			fflag = 1;
			add_compunit(CU_FILE, optarg);
			break;
		case 'i':
			inplace = optarg;
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
		"usage: sed script [-Ean] [-i extension] [file ...]",
		"       sed [-an] [-i extension] [-e script] ... [-f script_file] ... [file ...]");
	exit(1);
}

/*
 * Like fgets, but go through the chain of compilation units chaining them
 * together.  Empty strings and files are ignored.
 */
char *
cu_fgets(buf, n, more)
	char *buf;
	int n;
	int *more;
{
	static enum {ST_EOF, ST_FILE, ST_STRING} state = ST_EOF;
	static FILE *f;		/* Current open file */
	static char *s;		/* Current pointer inside string */
	static char string_ident[30];
	char *p;

again:
	switch (state) {
	case ST_EOF:
		if (script == NULL) {
			if (more != NULL)
				*more = 0;
			return (NULL);
		}
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
			if (more != NULL)
				*more = !feof(f);
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
				if (more != NULL)
					*more = 1;
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
					if (more != NULL)
						*more = 0;
					return (buf);
				}
			case '\n':
				*p++ = '\n';
				*p = '\0';
				s++;
				linenum++;
				if (more != NULL)
					*more = 0;
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
				if (inplace != NULL)
					errx(1, "-i may not be used with stdin");
				f = stdin;
				fname = "stdin";
			} else {
				if (inplace != NULL) {
					if (inplace_edit(&files->fname) == -1)
						continue;
				}
				fname = files->fname;
				if ((f = fopen(fname, "r")) == NULL)
					err(1, "%s", fname);
				if (inplace != NULL && *inplace == '\0')
					unlink(fname);
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
			if (inplace != NULL)
				errx(1, "-i may not be used with stdin");
			f = stdin;
			fname = "stdin";
		} else {
			if (inplace != NULL) {
				if (inplace_edit(&files->fname) == -1)
					continue;
			}
			fname = files->fname;
			if ((f = fopen(fname, "r")) == NULL)
				err(1, "%s", fname);
			if (inplace != NULL && *inplace == '\0')
				unlink(fname);
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

	if ((cu = malloc(sizeof(struct s_compunit))) == NULL)
		err(1, "malloc");
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

	if ((fp = malloc(sizeof(struct s_flist))) == NULL)
		err(1, "malloc");
	fp->next = NULL;
	*fl_nextp = fp;
	fp->fname = s;
	fl_nextp = &fp->next;
}

/*
 * Modify a pointer to a filename for inplace editing and reopen stdout
 */
static int
inplace_edit(filename)
	char **filename;
{
	struct stat orig;
	int input, output;
	char backup[MAXPATHLEN];
	char *buffer;

	if (lstat(*filename, &orig) == -1)
		err(1, "lstat");
	if ((orig.st_mode & S_IFREG) == 0) {
		warnx("cannot inplace edit %s, not a regular file", *filename);
		return -1;
	}

	if (*inplace == '\0') {
		char template[] = "/tmp/sed.XXXXXXXXXX";

		if (mktemp(template) == NULL)
			err(1, "mktemp");
		strlcpy(backup, template, MAXPATHLEN);
	} else {
		strlcpy(backup, *filename, MAXPATHLEN);
		strlcat(backup, inplace, MAXPATHLEN);
	}

	input = open(*filename, O_RDONLY);
	if (input == -1)
		err(1, "open(%s)", *filename);
	output = open(backup, O_WRONLY|O_CREAT);
	if (output == -1)
		err(1, "open(%s)", backup);
	if (fchmod(output, orig.st_mode & ~S_IFMT) == -1)
		err(1, "chmod");
	buffer = malloc(orig.st_size);
	if (buffer == NULL)
		errx(1, "malloc failed");
	if (read(input, buffer, orig.st_size) == -1)
		err(1, "read");
	if (write(output, buffer, orig.st_size) == -1)
		err(1, "write");
	close(input);
	close(output);
	freopen(*filename, "w", stdout);
	*filename = strdup(backup);
	return 0;
}
