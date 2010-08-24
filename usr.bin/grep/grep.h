/*	$OpenBSD: grep.h,v 1.15 2010/04/05 03:03:55 tedu Exp $	*/
/*	$FreeBSD$	*/

/*-
 * Copyright (c) 1999 James Howard and Dag-Erling Coïdan Smørgrav
 * Copyright (c) 2008-2009 Gabor Kovesdan <gabor@FreeBSD.org>
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

#include <bzlib.h>
#include <limits.h>
#include <regex.h>
#include <stdbool.h>
#include <stdio.h>
#include <zlib.h>

#ifdef WITHOUT_NLS
#define getstr(n)	 errstr[n]
#else
#include <nl_types.h>

extern nl_catd		 catalog;
#define getstr(n)	 catgets(catalog, 1, n, errstr[n])
#endif

extern const char		*errstr[];

#define VERSION		"2.5.1-FreeBSD"

#define GREP_FIXED	0
#define GREP_BASIC	1
#define GREP_EXTENDED	2

#define BINFILE_BIN	0
#define BINFILE_SKIP	1
#define BINFILE_TEXT	2

#define FILE_STDIO	0
#define FILE_GZIP	1
#define FILE_BZIP	2

#define DIR_READ	0
#define DIR_SKIP	1
#define DIR_RECURSE	2

#define DEV_READ	0
#define DEV_SKIP	1

#define LINK_READ	0
#define LINK_EXPLICIT	1
#define LINK_SKIP	2

#define EXCL_PAT	0
#define INCL_PAT	1

#define MAX_LINE_MATCHES	32

struct file {
	int		 fd;
	bool		 binary;
};

struct str {
	off_t		 off;
	size_t		 len;
	char		*dat;
	char		*file;
	int		 line_no;
};

struct epat {
	char		*pat;
	int		 mode;
};

typedef struct {
	size_t		 len;
	unsigned char	*pattern;
	int		 qsBc[UCHAR_MAX + 1];
	/* flags */
	bool		 bol;
	bool		 eol;
	bool		 reversed;
} fastgrep_t;

/* Flags passed to regcomp() and regexec() */
extern int	 cflags, eflags;

/* Command line flags */
extern bool	 Eflag, Fflag, Gflag, Hflag, Lflag,
		 bflag, cflag, hflag, iflag, lflag, mflag, nflag, oflag,
		 qflag, sflag, vflag, wflag, xflag;
extern bool	 dexclude, dinclude, fexclude, finclude, lbflag, nullflag;
extern unsigned long long Aflag, Bflag, mcount;
extern char	*label;
extern const char *color;
extern int	 binbehave, devbehave, dirbehave, filebehave, grepbehave, linkbehave;

extern bool	 first, matchall, notfound, prev;
extern int	 tail;
extern unsigned int dpatterns, fpatterns, patterns;
extern char    **pattern;
extern struct epat *dpattern, *fpattern;
extern regex_t	*er_pattern, *r_pattern;
extern fastgrep_t *fg_pattern;

/* For regex errors  */
#define RE_ERROR_BUF	512
extern char	 re_error[RE_ERROR_BUF + 1];	/* Seems big enough */

/* util.c */
bool	 file_matching(const char *fname);
int	 procfile(const char *fn);
int	 grep_tree(char **argv);
void	*grep_malloc(size_t size);
void	*grep_calloc(size_t nmemb, size_t size);
void	*grep_realloc(void *ptr, size_t size);
char	*grep_strdup(const char *str);
void	 printline(struct str *line, int sep, regmatch_t *matches, int m);

/* queue.c */
void	 enqueue(struct str *x);
void	 printqueue(void);
void	 clearqueue(void);

/* file.c */
void		 grep_close(struct file *f);
struct file	*grep_open(const char *path);
char		*grep_fgetln(struct file *f, size_t *len);

/* fastgrep.c */
int		 fastcomp(fastgrep_t *, const char *);
void		 fgrepcomp(fastgrep_t *, const char *);
int		 grep_search(fastgrep_t *, const unsigned char *, size_t, regmatch_t *);
