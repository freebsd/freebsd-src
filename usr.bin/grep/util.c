/*	$NetBSD: util.c,v 1.9 2011/02/27 17:33:37 joerg Exp $	*/
/*	$FreeBSD$	*/
/*	$OpenBSD: util.c,v 1.39 2010/07/02 22:18:03 tedu Exp $	*/

/*-
 * Copyright (c) 1999 James Howard and Dag-Erling Coïdan Smørgrav
 * Copyright (C) 2008-2010 Gabor Kovesdan <gabor@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
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

#include <sys/stat.h>
#include <sys/types.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fnmatch.h>
#include <fts.h>
#include <libgen.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wchar.h>
#include <wctype.h>

#ifndef WITHOUT_FASTMATCH
#include "fastmatch.h"
#endif
#include "grep.h"

static bool	 first_match = true;

/*
 * Parsing context; used to hold things like matches made and
 * other useful bits
 */
struct parsec {
	regmatch_t	matches[MAX_LINE_MATCHES];	/* Matches made */
	struct str	ln;				/* Current line */
	size_t		lnstart;			/* Position in line */
	size_t		matchidx;			/* Latest match index */
	int		printed;			/* Metadata printed? */
	bool		binary;				/* Binary file? */
};


static int procline(struct parsec *pc);
static void printline(struct parsec *pc, int sep);
static void printline_metadata(struct str *line, int sep);

bool
file_matching(const char *fname)
{
	char *fname_base, *fname_buf;
	bool ret;

	ret = finclude ? false : true;
	fname_buf = strdup(fname);
	if (fname_buf == NULL)
		err(2, "strdup");
	fname_base = basename(fname_buf);

	for (unsigned int i = 0; i < fpatterns; ++i) {
		if (fnmatch(fpattern[i].pat, fname, 0) == 0 ||
		    fnmatch(fpattern[i].pat, fname_base, 0) == 0) {
			if (fpattern[i].mode == EXCL_PAT) {
				ret = false;
				break;
			} else
				ret = true;
		}
	}
	free(fname_buf);
	return (ret);
}

static inline bool
dir_matching(const char *dname)
{
	bool ret;

	ret = dinclude ? false : true;

	for (unsigned int i = 0; i < dpatterns; ++i) {
		if (dname != NULL &&
		    fnmatch(dpattern[i].pat, dname, 0) == 0) {
			if (dpattern[i].mode == EXCL_PAT)
				return (false);
			else
				ret = true;
		}
	}
	return (ret);
}

/*
 * Processes a directory when a recursive search is performed with
 * the -R option.  Each appropriate file is passed to procfile().
 */
int
grep_tree(char **argv)
{
	FTS *fts;
	FTSENT *p;
	int c, fts_flags;
	bool ok;
	const char *wd[] = { ".", NULL };

	c = fts_flags = 0;

	switch(linkbehave) {
	case LINK_EXPLICIT:
		fts_flags = FTS_COMFOLLOW;
		break;
	case LINK_SKIP:
		fts_flags = FTS_PHYSICAL;
		break;
	default:
		fts_flags = FTS_LOGICAL;
			
	}

	fts_flags |= FTS_NOSTAT | FTS_NOCHDIR;

	fts = fts_open((argv[0] == NULL) ?
	    __DECONST(char * const *, wd) : argv, fts_flags, NULL);
	if (fts == NULL)
		err(2, "fts_open");
	while ((p = fts_read(fts)) != NULL) {
		switch (p->fts_info) {
		case FTS_DNR:
			/* FALLTHROUGH */
		case FTS_ERR:
			file_err = true;
			if(!sflag)
				warnx("%s: %s", p->fts_path, strerror(p->fts_errno));
			break;
		case FTS_D:
			/* FALLTHROUGH */
		case FTS_DP:
			if (dexclude || dinclude)
				if (!dir_matching(p->fts_name) ||
				    !dir_matching(p->fts_path))
					fts_set(fts, p, FTS_SKIP);
			break;
		case FTS_DC:
			/* Print a warning for recursive directory loop */
			warnx("warning: %s: recursive directory loop",
				p->fts_path);
			break;
		default:
			/* Check for file exclusion/inclusion */
			ok = true;
			if (fexclude || finclude)
				ok &= file_matching(p->fts_path);

			if (ok)
				c += procfile(p->fts_path);
			break;
		}
	}

	fts_close(fts);
	return (c);
}

/*
 * Opens a file and processes it.  Each file is processed line-by-line
 * passing the lines to procline().
 */
int
procfile(const char *fn)
{
	struct parsec pc;
	long long tail;
	struct file *f;
	struct stat sb;
	struct str *ln;
	mode_t s;
	int c, last_outed, t;
	bool doctx, printmatch, same_file;

	if (strcmp(fn, "-") == 0) {
		fn = label != NULL ? label : getstr(1);
		f = grep_open(NULL);
	} else {
		if (!stat(fn, &sb)) {
			/* Check if we need to process the file */
			s = sb.st_mode & S_IFMT;
			if (s == S_IFDIR && dirbehave == DIR_SKIP)
				return (0);
			if ((s == S_IFIFO || s == S_IFCHR || s == S_IFBLK
				|| s == S_IFSOCK) && devbehave == DEV_SKIP)
					return (0);
		}
		f = grep_open(fn);
	}
	if (f == NULL) {
		file_err = true;
		if (!sflag)
			warn("%s", fn);
		return (0);
	}

	/* Convenience */
	ln = &pc.ln;
	pc.ln.file = grep_malloc(strlen(fn) + 1);
	strcpy(pc.ln.file, fn);
	pc.ln.line_no = 0;
	pc.ln.len = 0;
	pc.ln.boff = 0;
	pc.ln.off = -1;
	pc.binary = f->binary;
	pc.printed = 0;
	tail = 0;
	last_outed = 0;
	same_file = false;
	doctx = false;
	printmatch = true;
	if ((pc.binary && binbehave == BINFILE_BIN) || cflag || qflag ||
	    lflag || Lflag)
		printmatch = false;
	if (printmatch && (Aflag != 0 || Bflag != 0))
		doctx = true;
	mcount = mlimit;

	for (c = 0;  c == 0 || !(lflag || qflag); ) {
		/* Reset per-line statistics */
		pc.printed = 0;
		pc.matchidx = 0;
		pc.lnstart = 0;
		pc.ln.boff = 0;
		pc.ln.off += pc.ln.len + 1;
		if ((pc.ln.dat = grep_fgetln(f, &pc.ln.len)) == NULL ||
		    pc.ln.len == 0) {
			if (pc.ln.line_no == 0 && matchall)
				/*
				 * An empty file with an empty pattern and the
				 * -w flag does not match
				 */
				exit(matchall && wflag ? 1 : 0);
			else
				break;
		}

		if (pc.ln.len > 0 && pc.ln.dat[pc.ln.len - 1] == fileeol)
			--pc.ln.len;
		pc.ln.line_no++;

		/* Return if we need to skip a binary file */
		if (pc.binary && binbehave == BINFILE_SKIP) {
			grep_close(f);
			free(pc.ln.file);
			free(f);
			return (0);
		}

		if ((t = procline(&pc)) == 0)
			++c;

		/* Deal with any -B context or context separators */
		if (t == 0 && doctx) {
			if (!first_match && (!same_file || last_outed > 0))
				printf("--\n");
			if (Bflag > 0)
				printqueue();
			tail = Aflag;
		}
		/* Print the matching line, but only if not quiet/binary */
		if (t == 0 && printmatch) {
			printline(&pc, ':');
			while (pc.matchidx >= MAX_LINE_MATCHES) {
				/* Reset matchidx and try again */
				pc.matchidx = 0;
				if (procline(&pc) == 0)
					printline(&pc, ':');
				else
					break;
			}
			first_match = false;
			same_file = true;
			last_outed = 0;
		}
		if (t != 0 && doctx) {
			/* Deal with any -A context */
			if (tail > 0) {
				grep_printline(&pc.ln, '-');
				tail--;
				if (Bflag > 0)
					clearqueue();
			} else {
				/*
				 * Enqueue non-matching lines for -B context.
				 * If we're not actually doing -B context or if
				 * the enqueue resulted in a line being rotated
				 * out, then go ahead and increment last_outed
				 * to signify a gap between context/match.
				 */
				if (Bflag == 0 || (Bflag > 0 && enqueue(ln)))
					++last_outed;
			}
		}

		/* Count the matches if we have a match limit */
		if (t == 0 && mflag) {
			--mcount;
			if (mflag && mcount <= 0)
				break;
		}

	}
	if (Bflag > 0)
		clearqueue();
	grep_close(f);

	if (cflag) {
		if (!hflag)
			printf("%s:", pc.ln.file);
		printf("%u\n", c);
	}
	if (lflag && !qflag && c != 0)
		printf("%s%c", fn, nullflag ? 0 : '\n');
	if (Lflag && !qflag && c == 0)
		printf("%s%c", fn, nullflag ? 0 : '\n');
	if (c && !cflag && !lflag && !Lflag &&
	    binbehave == BINFILE_BIN && f->binary && !qflag)
		printf(getstr(8), fn);

	free(pc.ln.file);
	free(f);
	return (c);
}

#define iswword(x)	(iswalnum((x)) || (x) == L'_')

/*
 * Processes a line comparing it with the specified patterns.  Each pattern
 * is looped to be compared along with the full string, saving each and every
 * match, which is necessary to colorize the output and to count the
 * matches.  The matching lines are passed to printline() to display the
 * appropriate output.
 */
static int
procline(struct parsec *pc)
{
	regmatch_t pmatch, lastmatch, chkmatch;
	wchar_t wbegin, wend;
	size_t st, nst;
	unsigned int i;
	int c = 0, r = 0, lastmatches = 0, leflags = eflags;
	size_t startm = 0, matchidx;
	unsigned int retry;

	matchidx = pc->matchidx;

	/* Special case: empty pattern with -w flag, check first character */
	if (matchall && wflag) {
		if (pc->ln.len == 0)
			return (0);
		wend = L' ';
		if (sscanf(&pc->ln.dat[0], "%lc", &wend) != 1 || iswword(wend))
			return (1);
		else
			return (0);
	} else if (matchall)
		return (0);

	st = pc->lnstart;
	nst = 0;
	/* Initialize to avoid a false positive warning from GCC. */
	lastmatch.rm_so = lastmatch.rm_eo = 0;

	/* Loop to process the whole line */
	while (st <= pc->ln.len) {
		lastmatches = 0;
		startm = matchidx;
		retry = 0;
		if (st > 0)
			leflags |= REG_NOTBOL;
		/* Loop to compare with all the patterns */
		for (i = 0; i < patterns; i++) {
			pmatch.rm_so = st;
			pmatch.rm_eo = pc->ln.len;
#ifndef WITHOUT_FASTMATCH
			if (fg_pattern[i].pattern)
				r = fastexec(&fg_pattern[i],
				    pc->ln.dat, 1, &pmatch, leflags);
			else
#endif
				r = regexec(&r_pattern[i], pc->ln.dat, 1,
				    &pmatch, leflags);
			if (r != 0)
				continue;
			/* Check for full match */
			if (xflag && (pmatch.rm_so != 0 ||
			    (size_t)pmatch.rm_eo != pc->ln.len))
				continue;
			/* Check for whole word match */
#ifndef WITHOUT_FASTMATCH
			if (wflag || fg_pattern[i].word) {
#else
			if (wflag) {
#endif
				wbegin = wend = L' ';
				if (pmatch.rm_so != 0 &&
				    sscanf(&pc->ln.dat[pmatch.rm_so - 1],
				    "%lc", &wbegin) != 1)
					r = REG_NOMATCH;
				else if ((size_t)pmatch.rm_eo !=
				    pc->ln.len &&
				    sscanf(&pc->ln.dat[pmatch.rm_eo],
				    "%lc", &wend) != 1)
					r = REG_NOMATCH;
				else if (iswword(wbegin) ||
				    iswword(wend))
					r = REG_NOMATCH;
				/*
				 * If we're doing whole word matching and we
				 * matched once, then we should try the pattern
				 * again after advancing just past the start of
				 * the earliest match. This allows the pattern
				 * to  match later on in the line and possibly
				 * still match a whole word.
				 */
				if (r == REG_NOMATCH &&
				    (retry == pc->lnstart ||
				    pmatch.rm_so + 1 < retry))
					retry = pmatch.rm_so + 1;
				if (r == REG_NOMATCH)
					continue;
			}
			lastmatches++;
			lastmatch = pmatch;

			if (matchidx == 0)
				c++;

			/*
			 * Replace previous match if the new one is earlier
			 * and/or longer. This will lead to some amount of
			 * extra work if -o/--color are specified, but it's
			 * worth it from a correctness point of view.
			 */
			if (matchidx > startm) {
				chkmatch = pc->matches[matchidx - 1];
				if (pmatch.rm_so < chkmatch.rm_so ||
				    (pmatch.rm_so == chkmatch.rm_so &&
				    (pmatch.rm_eo - pmatch.rm_so) >
				    (chkmatch.rm_eo - chkmatch.rm_so))) {
					pc->matches[matchidx - 1] = pmatch;
					nst = pmatch.rm_eo;
				}
			} else {
				/* Advance as normal if not */
				pc->matches[matchidx++] = pmatch;
				nst = pmatch.rm_eo;
			}
			/* avoid excessive matching - skip further patterns */
			if ((color == NULL && !oflag) || qflag || lflag ||
			    matchidx >= MAX_LINE_MATCHES) {
				pc->lnstart = nst;
				lastmatches = 0;
				break;
			}
		}

		/*
		 * Advance to just past the start of the earliest match, try
		 * again just in case we still have a chance to match later in
		 * the string.
		 */
		if (lastmatches == 0 && retry > pc->lnstart) {
			st = retry;
			continue;
		}

		/* One pass if we are not recording matches */
		if (!wflag && ((color == NULL && !oflag) || qflag || lflag || Lflag))
			break;

		/* If we didn't have any matches or REG_NOSUB set */
		if (lastmatches == 0 || (cflags & REG_NOSUB))
			nst = pc->ln.len;

		if (lastmatches == 0)
			/* No matches */
			break;
		else if (st == nst && lastmatch.rm_so == lastmatch.rm_eo)
			/* Zero-length match -- advance one more so we don't get stuck */
			nst++;

		/* Advance st based on previous matches */
		st = nst;
		pc->lnstart = st;
	}

	/* Reflect the new matchidx in the context */
	pc->matchidx = matchidx;
	if (vflag)
		c = !c;
	return (c ? 0 : 1);
}

/*
 * Safe malloc() for internal use.
 */
void *
grep_malloc(size_t size)
{
	void *ptr;

	if ((ptr = malloc(size)) == NULL)
		err(2, "malloc");
	return (ptr);
}

/*
 * Safe calloc() for internal use.
 */
void *
grep_calloc(size_t nmemb, size_t size)
{
	void *ptr;

	if ((ptr = calloc(nmemb, size)) == NULL)
		err(2, "calloc");
	return (ptr);
}

/*
 * Safe realloc() for internal use.
 */
void *
grep_realloc(void *ptr, size_t size)
{

	if ((ptr = realloc(ptr, size)) == NULL)
		err(2, "realloc");
	return (ptr);
}

/*
 * Safe strdup() for internal use.
 */
char *
grep_strdup(const char *str)
{
	char *ret;

	if ((ret = strdup(str)) == NULL)
		err(2, "strdup");
	return (ret);
}

/*
 * Print an entire line as-is, there are no inline matches to consider. This is
 * used for printing context.
 */
void grep_printline(struct str *line, int sep) {
	printline_metadata(line, sep);
	fwrite(line->dat, line->len, 1, stdout);
	putchar(fileeol);
}

static void
printline_metadata(struct str *line, int sep)
{
	bool printsep;

	printsep = false;
	if (!hflag) {
		if (!nullflag) {
			fputs(line->file, stdout);
			printsep = true;
		} else {
			printf("%s", line->file);
			putchar(0);
		}
	}
	if (nflag) {
		if (printsep)
			putchar(sep);
		printf("%d", line->line_no);
		printsep = true;
	}
	if (bflag) {
		if (printsep)
			putchar(sep);
		printf("%lld", (long long)(line->off + line->boff));
		printsep = true;
	}
	if (printsep)
		putchar(sep);
}

/*
 * Prints a matching line according to the command line options.
 */
static void
printline(struct parsec *pc, int sep)
{
	size_t a = 0;
	size_t i, matchidx;
	regmatch_t match;

	/* If matchall, everything matches but don't actually print for -o */
	if (oflag && matchall)
		return;

	matchidx = pc->matchidx;

	/* --color and -o */
	if ((oflag || color) && matchidx > 0) {
		/* Only print metadata once per line if --color */
		if (!oflag && pc->printed == 0)
			printline_metadata(&pc->ln, sep);
		for (i = 0; i < matchidx; i++) {
			match = pc->matches[i];
			/* Don't output zero length matches */
			if (match.rm_so == match.rm_eo)
				continue;
			/*
			 * Metadata is printed on a per-line basis, so every
			 * match gets file metadata with the -o flag.
			 */
			if (oflag) {
				pc->ln.boff = match.rm_so;
				printline_metadata(&pc->ln, sep);
			} else
				fwrite(pc->ln.dat + a, match.rm_so - a, 1,
				    stdout);
			if (color)
				fprintf(stdout, "\33[%sm\33[K", color);
			fwrite(pc->ln.dat + match.rm_so,
			    match.rm_eo - match.rm_so, 1, stdout);
			if (color)
				fprintf(stdout, "\33[m\33[K");
			a = match.rm_eo;
			if (oflag)
				putchar('\n');
		}
		if (!oflag) {
			if (pc->ln.len - a > 0)
				fwrite(pc->ln.dat + a, pc->ln.len - a, 1,
				    stdout);
			putchar('\n');
		}
	} else
		grep_printline(&pc->ln, sep);
	pc->printed++;
}
