/*	$Id: main.c,v 1.167 2012/11/19 17:22:26 schwarze Exp $ */
/*
 * Copyright (c) 2008, 2009, 2010, 2011 Kristaps Dzonsons <kristaps@bsd.lv>
 * Copyright (c) 2010, 2011, 2012 Ingo Schwarze <schwarze@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mandoc.h"
#include "main.h"
#include "mdoc.h"
#include "man.h"

#if !defined(__GNUC__) || (__GNUC__ < 2)
# if !defined(lint)
#  define __attribute__(x)
# endif
#endif /* !defined(__GNUC__) || (__GNUC__ < 2) */

typedef	void		(*out_mdoc)(void *, const struct mdoc *);
typedef	void		(*out_man)(void *, const struct man *);
typedef	void		(*out_free)(void *);

enum	outt {
	OUTT_ASCII = 0,	/* -Tascii */
	OUTT_LOCALE,	/* -Tlocale */
	OUTT_UTF8,	/* -Tutf8 */
	OUTT_TREE,	/* -Ttree */
	OUTT_MAN,	/* -Tman */
	OUTT_HTML,	/* -Thtml */
	OUTT_XHTML,	/* -Txhtml */
	OUTT_LINT,	/* -Tlint */
	OUTT_PS,	/* -Tps */
	OUTT_PDF	/* -Tpdf */
};

struct	curparse {
	struct mparse	 *mp;
	enum mandoclevel  wlevel;	/* ignore messages below this */
	int		  wstop;	/* stop after a file with a warning */
	enum outt	  outtype; 	/* which output to use */
	out_mdoc	  outmdoc;	/* mdoc output ptr */
	out_man	  	  outman;	/* man output ptr */
	out_free	  outfree;	/* free output ptr */
	void		 *outdata;	/* data for output */
	char		  outopts[BUFSIZ]; /* buf of output opts */
};

static	int		  moptions(enum mparset *, char *);
static	void		  mmsg(enum mandocerr, enum mandoclevel,
				const char *, int, int, const char *);
static	void		  parse(struct curparse *, int, 
				const char *, enum mandoclevel *);
static	int		  toptions(struct curparse *, char *);
static	void		  usage(void) __attribute__((noreturn));
static	void		  version(void) __attribute__((noreturn));
static	int		  woptions(struct curparse *, char *);

static	const char	 *progname;

int
main(int argc, char *argv[])
{
	int		 c;
	struct curparse	 curp;
	enum mparset	 type;
	enum mandoclevel rc;
	char		*defos;

	progname = strrchr(argv[0], '/');
	if (progname == NULL)
		progname = argv[0];
	else
		++progname;

	memset(&curp, 0, sizeof(struct curparse));

	type = MPARSE_AUTO;
	curp.outtype = OUTT_ASCII;
	curp.wlevel  = MANDOCLEVEL_FATAL;
	defos = NULL;

	/* LINTED */
	while (-1 != (c = getopt(argc, argv, "I:m:O:T:VW:")))
		switch (c) {
		case ('I'):
			if (strncmp(optarg, "os=", 3)) {
				fprintf(stderr, "-I%s: Bad argument\n",
						optarg);
				return((int)MANDOCLEVEL_BADARG);
			}
			if (defos) {
				fprintf(stderr, "-I%s: Duplicate argument\n",
						optarg);
				return((int)MANDOCLEVEL_BADARG);
			}
			defos = mandoc_strdup(optarg + 3);
			break;
		case ('m'):
			if ( ! moptions(&type, optarg))
				return((int)MANDOCLEVEL_BADARG);
			break;
		case ('O'):
			(void)strlcat(curp.outopts, optarg, BUFSIZ);
			(void)strlcat(curp.outopts, ",", BUFSIZ);
			break;
		case ('T'):
			if ( ! toptions(&curp, optarg))
				return((int)MANDOCLEVEL_BADARG);
			break;
		case ('W'):
			if ( ! woptions(&curp, optarg))
				return((int)MANDOCLEVEL_BADARG);
			break;
		case ('V'):
			version();
			/* NOTREACHED */
		default:
			usage();
			/* NOTREACHED */
		}

	curp.mp = mparse_alloc(type, curp.wlevel, mmsg, &curp, defos);

	/*
	 * Conditionally start up the lookaside buffer before parsing.
	 */
	if (OUTT_MAN == curp.outtype)
		mparse_keep(curp.mp);

	argc -= optind;
	argv += optind;

	rc = MANDOCLEVEL_OK;

	if (NULL == *argv)
		parse(&curp, STDIN_FILENO, "<stdin>", &rc);

	while (*argv) {
		parse(&curp, -1, *argv, &rc);
		if (MANDOCLEVEL_OK != rc && curp.wstop)
			break;
		++argv;
	}

	if (curp.outfree)
		(*curp.outfree)(curp.outdata);
	if (curp.mp)
		mparse_free(curp.mp);
	free(defos);

	return((int)rc);
}

static void
version(void)
{

	printf("%s %s\n", progname, VERSION);
	exit((int)MANDOCLEVEL_OK);
}

static void
usage(void)
{

	fprintf(stderr, "usage: %s "
			"[-V] "
			"[-Ios=name] "
			"[-mformat] "
			"[-Ooption] "
			"[-Toutput] "
			"[-Wlevel]\n"
			"\t      [file ...]\n", 
			progname);

	exit((int)MANDOCLEVEL_BADARG);
}

static void
parse(struct curparse *curp, int fd, 
		const char *file, enum mandoclevel *level)
{
	enum mandoclevel  rc;
	struct mdoc	 *mdoc;
	struct man	 *man;

	/* Begin by parsing the file itself. */

	assert(file);
	assert(fd >= -1);

	rc = mparse_readfd(curp->mp, fd, file);

	/* Stop immediately if the parse has failed. */

	if (MANDOCLEVEL_FATAL <= rc)
		goto cleanup;

	/*
	 * With -Wstop and warnings or errors of at least the requested
	 * level, do not produce output.
	 */

	if (MANDOCLEVEL_OK != rc && curp->wstop)
		goto cleanup;

	/* If unset, allocate output dev now (if applicable). */

	if ( ! (curp->outman && curp->outmdoc)) {
		switch (curp->outtype) {
		case (OUTT_XHTML):
			curp->outdata = xhtml_alloc(curp->outopts);
			curp->outfree = html_free;
			break;
		case (OUTT_HTML):
			curp->outdata = html_alloc(curp->outopts);
			curp->outfree = html_free;
			break;
		case (OUTT_UTF8):
			curp->outdata = utf8_alloc(curp->outopts);
			curp->outfree = ascii_free;
			break;
		case (OUTT_LOCALE):
			curp->outdata = locale_alloc(curp->outopts);
			curp->outfree = ascii_free;
			break;
		case (OUTT_ASCII):
			curp->outdata = ascii_alloc(curp->outopts);
			curp->outfree = ascii_free;
			break;
		case (OUTT_PDF):
			curp->outdata = pdf_alloc(curp->outopts);
			curp->outfree = pspdf_free;
			break;
		case (OUTT_PS):
			curp->outdata = ps_alloc(curp->outopts);
			curp->outfree = pspdf_free;
			break;
		default:
			break;
		}

		switch (curp->outtype) {
		case (OUTT_HTML):
			/* FALLTHROUGH */
		case (OUTT_XHTML):
			curp->outman = html_man;
			curp->outmdoc = html_mdoc;
			break;
		case (OUTT_TREE):
			curp->outman = tree_man;
			curp->outmdoc = tree_mdoc;
			break;
		case (OUTT_MAN):
			curp->outmdoc = man_mdoc;
			curp->outman = man_man;
			break;
		case (OUTT_PDF):
			/* FALLTHROUGH */
		case (OUTT_ASCII):
			/* FALLTHROUGH */
		case (OUTT_UTF8):
			/* FALLTHROUGH */
		case (OUTT_LOCALE):
			/* FALLTHROUGH */
		case (OUTT_PS):
			curp->outman = terminal_man;
			curp->outmdoc = terminal_mdoc;
			break;
		default:
			break;
		}
	}

	mparse_result(curp->mp, &mdoc, &man);

	/* Execute the out device, if it exists. */

	if (man && curp->outman)
		(*curp->outman)(curp->outdata, man);
	if (mdoc && curp->outmdoc)
		(*curp->outmdoc)(curp->outdata, mdoc);

 cleanup:

	mparse_reset(curp->mp);

	if (*level < rc)
		*level = rc;
}

static int
moptions(enum mparset *tflags, char *arg)
{

	if (0 == strcmp(arg, "doc"))
		*tflags = MPARSE_MDOC;
	else if (0 == strcmp(arg, "andoc"))
		*tflags = MPARSE_AUTO;
	else if (0 == strcmp(arg, "an"))
		*tflags = MPARSE_MAN;
	else {
		fprintf(stderr, "%s: Bad argument\n", arg);
		return(0);
	}

	return(1);
}

static int
toptions(struct curparse *curp, char *arg)
{

	if (0 == strcmp(arg, "ascii"))
		curp->outtype = OUTT_ASCII;
	else if (0 == strcmp(arg, "lint")) {
		curp->outtype = OUTT_LINT;
		curp->wlevel  = MANDOCLEVEL_WARNING;
	} else if (0 == strcmp(arg, "tree"))
		curp->outtype = OUTT_TREE;
	else if (0 == strcmp(arg, "man"))
		curp->outtype = OUTT_MAN;
	else if (0 == strcmp(arg, "html"))
		curp->outtype = OUTT_HTML;
	else if (0 == strcmp(arg, "utf8"))
		curp->outtype = OUTT_UTF8;
	else if (0 == strcmp(arg, "locale"))
		curp->outtype = OUTT_LOCALE;
	else if (0 == strcmp(arg, "xhtml"))
		curp->outtype = OUTT_XHTML;
	else if (0 == strcmp(arg, "ps"))
		curp->outtype = OUTT_PS;
	else if (0 == strcmp(arg, "pdf"))
		curp->outtype = OUTT_PDF;
	else {
		fprintf(stderr, "%s: Bad argument\n", arg);
		return(0);
	}

	return(1);
}

static int
woptions(struct curparse *curp, char *arg)
{
	char		*v, *o;
	const char	*toks[6]; 

	toks[0] = "stop";
	toks[1] = "all";
	toks[2] = "warning";
	toks[3] = "error";
	toks[4] = "fatal";
	toks[5] = NULL;

	while (*arg) {
		o = arg;
		switch (getsubopt(&arg, UNCONST(toks), &v)) {
		case (0):
			curp->wstop = 1;
			break;
		case (1):
			/* FALLTHROUGH */
		case (2):
			curp->wlevel = MANDOCLEVEL_WARNING;
			break;
		case (3):
			curp->wlevel = MANDOCLEVEL_ERROR;
			break;
		case (4):
			curp->wlevel = MANDOCLEVEL_FATAL;
			break;
		default:
			fprintf(stderr, "-W%s: Bad argument\n", o);
			return(0);
		}
	}

	return(1);
}

static void
mmsg(enum mandocerr t, enum mandoclevel lvl, 
		const char *file, int line, int col, const char *msg)
{

	fprintf(stderr, "%s:%d:%d: %s: %s", 
			file, line, col + 1, 
			mparse_strlevel(lvl),
			mparse_strerror(t));

	if (msg)
		fprintf(stderr, ": %s", msg);

	fputc('\n', stderr);
}
