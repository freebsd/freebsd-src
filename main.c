/* $Id: main.c,v 1.358 2021/09/04 22:38:46 schwarze Exp $ */
/*
 * Copyright (c) 2010-2012, 2014-2021 Ingo Schwarze <schwarze@openbsd.org>
 * Copyright (c) 2008-2012 Kristaps Dzonsons <kristaps@bsd.lv>
 * Copyright (c) 2010 Joerg Sonnenberger <joerg@netbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHORS DISCLAIM ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Main program for mandoc(1), man(1), apropos(1), whatis(1), and help(1).
 */
#include "config.h"

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/param.h>	/* MACHINE */
#include <sys/stat.h>
#include <sys/wait.h>

#include <assert.h>
#include <ctype.h>
#if HAVE_ERR
#include <err.h>
#endif
#include <errno.h>
#include <fcntl.h>
#include <glob.h>
#include <limits.h>
#if HAVE_SANDBOX_INIT
#include <sandbox.h>
#endif
#include <signal.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include "mandoc_aux.h"
#include "mandoc.h"
#include "mandoc_xr.h"
#include "roff.h"
#include "mdoc.h"
#include "man.h"
#include "mandoc_parse.h"
#include "tag.h"
#include "term_tag.h"
#include "main.h"
#include "manconf.h"
#include "mansearch.h"

enum	outmode {
	OUTMODE_DEF = 0,
	OUTMODE_FLN,
	OUTMODE_LST,
	OUTMODE_ALL,
	OUTMODE_ONE
};

enum	outt {
	OUTT_ASCII = 0,	/* -Tascii */
	OUTT_LOCALE,	/* -Tlocale */
	OUTT_UTF8,	/* -Tutf8 */
	OUTT_TREE,	/* -Ttree */
	OUTT_MAN,	/* -Tman */
	OUTT_HTML,	/* -Thtml */
	OUTT_MARKDOWN,	/* -Tmarkdown */
	OUTT_LINT,	/* -Tlint */
	OUTT_PS,	/* -Tps */
	OUTT_PDF	/* -Tpdf */
};

struct	outstate {
	struct tag_files *tag_files;	/* Tagging state variables. */
	void		 *outdata;	/* data for output */
	int		  use_pager;
	int		  wstop;	/* stop after a file with a warning */
	int		  had_output;	/* Some output was generated. */
	enum outt	  outtype;	/* which output to use */
};


int			  mandocdb(int, char *[]);

static	void		  check_xr(struct manpaths *);
static	void		  fs_append(char **, size_t, int,
				size_t, const char *, enum form,
				struct manpage **, size_t *);
static	int		  fs_lookup(const struct manpaths *, size_t,
				const char *, const char *, const char *,
				struct manpage **, size_t *);
static	int		  fs_search(const struct mansearch *,
				const struct manpaths *, const char *,
				struct manpage **, size_t *);
static	void		  glob_esc(char **, const char *, const char *);
static	void		  outdata_alloc(struct outstate *, struct manoutput *);
static	void		  parse(struct mparse *, int, const char *,
				struct outstate *, struct manconf *);
static	void		  passthrough(int, int);
static	void		  process_onefile(struct mparse *, struct manpage *,
				int, struct outstate *, struct manconf *);
static	void		  run_pager(struct outstate *, char *);
static	pid_t		  spawn_pager(struct outstate *, char *);
static	void		  usage(enum argmode) __attribute__((__noreturn__));
static	int		  woptions(char *, enum mandoc_os *, int *);

static	const int sec_prios[] = {1, 4, 5, 8, 6, 3, 7, 2, 9};
static	char		  help_arg[] = "help";
static	char		 *help_argv[] = {help_arg, NULL};


int
main(int argc, char *argv[])
{
	struct manconf	 conf;		/* Manpaths and output options. */
	struct outstate	 outst;		/* Output state. */
	struct winsize	 ws;		/* Result of ioctl(TIOCGWINSZ). */
	struct mansearch search;	/* Search options. */
	struct manpage	*res;		/* Complete list of search results. */
	struct manpage	*resn;		/* Search results for one name. */
	struct mparse	*mp;		/* Opaque parser object. */
	const char	*conf_file;	/* -C: alternate config file. */
	const char	*os_s;		/* -I: Operating system for display. */
	const char	*progname, *sec, *ep;
	char		*defpaths;	/* -M: override manpaths. */
	char		*auxpaths;	/* -m: additional manpaths. */
	char		*oarg;		/* -O: output option string. */
	char		*tagarg;	/* -O tag: default value. */
	unsigned char	*uc;
	size_t		 ressz;		/* Number of elements in res[]. */
	size_t		 resnsz;	/* Number of elements in resn[]. */
	size_t		 i, ib, ssz;
	int		 options;	/* Parser options. */
	int		 show_usage;	/* Invalid argument: give up. */
	int		 prio, best_prio;
	int		 startdir;
	int		 c;
	enum mandoc_os	 os_e;		/* Check base system conventions. */
	enum outmode	 outmode;	/* According to command line. */

#if HAVE_PROGNAME
	progname = getprogname();
#else
	if (argc < 1)
		progname = mandoc_strdup("mandoc");
	else if ((progname = strrchr(argv[0], '/')) == NULL)
		progname = argv[0];
	else
		++progname;
	setprogname(progname);
#endif

	mandoc_msg_setoutfile(stderr);
	if (strncmp(progname, "mandocdb", 8) == 0 ||
	    strcmp(progname, BINM_MAKEWHATIS) == 0)
		return mandocdb(argc, argv);

#if HAVE_PLEDGE
	if (pledge("stdio rpath wpath cpath tmppath tty proc exec", NULL) == -1) {
		mandoc_msg(MANDOCERR_PLEDGE, 0, 0, "%s", strerror(errno));
		return mandoc_msg_getrc();
	}
#endif
#if HAVE_SANDBOX_INIT
	if (sandbox_init(kSBXProfileNoInternet, SANDBOX_NAMED, NULL) == -1)
		errx((int)MANDOCLEVEL_SYSERR, "sandbox_init");
#endif

	/* Search options. */

	memset(&conf, 0, sizeof(conf));
	conf_file = NULL;
	defpaths = auxpaths = NULL;

	memset(&search, 0, sizeof(struct mansearch));
	search.outkey = "Nd";
	oarg = NULL;

	if (strcmp(progname, BINM_MAN) == 0)
		search.argmode = ARG_NAME;
	else if (strcmp(progname, BINM_APROPOS) == 0)
		search.argmode = ARG_EXPR;
	else if (strcmp(progname, BINM_WHATIS) == 0)
		search.argmode = ARG_WORD;
	else if (strncmp(progname, "help", 4) == 0)
		search.argmode = ARG_NAME;
	else
		search.argmode = ARG_FILE;

	/* Parser options. */

	options = MPARSE_SO | MPARSE_UTF8 | MPARSE_LATIN1;
	os_e = MANDOC_OS_OTHER;
	os_s = NULL;

	/* Formatter options. */

	memset(&outst, 0, sizeof(outst));
	outst.tag_files = NULL;
	outst.outtype = OUTT_LOCALE;
	outst.use_pager = 1;

	show_usage = 0;
	outmode = OUTMODE_DEF;

	while ((c = getopt(argc, argv,
	    "aC:cfhI:iK:klM:m:O:S:s:T:VW:w")) != -1) {
		if (c == 'i' && search.argmode == ARG_EXPR) {
			optind--;
			break;
		}
		switch (c) {
		case 'a':
			outmode = OUTMODE_ALL;
			break;
		case 'C':
			conf_file = optarg;
			break;
		case 'c':
			outst.use_pager = 0;
			break;
		case 'f':
			search.argmode = ARG_WORD;
			break;
		case 'h':
			conf.output.synopsisonly = 1;
			outst.use_pager = 0;
			outmode = OUTMODE_ALL;
			break;
		case 'I':
			if (strncmp(optarg, "os=", 3) != 0) {
				mandoc_msg(MANDOCERR_BADARG_BAD, 0, 0,
				    "-I %s", optarg);
				return mandoc_msg_getrc();
			}
			if (os_s != NULL) {
				mandoc_msg(MANDOCERR_BADARG_DUPE, 0, 0,
				    "-I %s", optarg);
				return mandoc_msg_getrc();
			}
			os_s = optarg + 3;
			break;
		case 'K':
			options &= ~(MPARSE_UTF8 | MPARSE_LATIN1);
			if (strcmp(optarg, "utf-8") == 0)
				options |=  MPARSE_UTF8;
			else if (strcmp(optarg, "iso-8859-1") == 0)
				options |=  MPARSE_LATIN1;
			else if (strcmp(optarg, "us-ascii") != 0) {
				mandoc_msg(MANDOCERR_BADARG_BAD, 0, 0,
				    "-K %s", optarg);
				return mandoc_msg_getrc();
			}
			break;
		case 'k':
			search.argmode = ARG_EXPR;
			break;
		case 'l':
			search.argmode = ARG_FILE;
			outmode = OUTMODE_ALL;
			break;
		case 'M':
			defpaths = optarg;
			break;
		case 'm':
			auxpaths = optarg;
			break;
		case 'O':
			oarg = optarg;
			break;
		case 'S':
			search.arch = optarg;
			break;
		case 's':
			search.sec = optarg;
			break;
		case 'T':
			if (strcmp(optarg, "ascii") == 0)
				outst.outtype = OUTT_ASCII;
			else if (strcmp(optarg, "lint") == 0) {
				outst.outtype = OUTT_LINT;
				mandoc_msg_setoutfile(stdout);
				mandoc_msg_setmin(MANDOCERR_BASE);
			} else if (strcmp(optarg, "tree") == 0)
				outst.outtype = OUTT_TREE;
			else if (strcmp(optarg, "man") == 0)
				outst.outtype = OUTT_MAN;
			else if (strcmp(optarg, "html") == 0)
				outst.outtype = OUTT_HTML;
			else if (strcmp(optarg, "markdown") == 0)
				outst.outtype = OUTT_MARKDOWN;
			else if (strcmp(optarg, "utf8") == 0)
				outst.outtype = OUTT_UTF8;
			else if (strcmp(optarg, "locale") == 0)
				outst.outtype = OUTT_LOCALE;
			else if (strcmp(optarg, "ps") == 0)
				outst.outtype = OUTT_PS;
			else if (strcmp(optarg, "pdf") == 0)
				outst.outtype = OUTT_PDF;
			else {
				mandoc_msg(MANDOCERR_BADARG_BAD, 0, 0,
				    "-T %s", optarg);
				return mandoc_msg_getrc();
			}
			break;
		case 'W':
			if (woptions(optarg, &os_e, &outst.wstop) == -1)
				return mandoc_msg_getrc();
			break;
		case 'w':
			outmode = OUTMODE_FLN;
			break;
		default:
			show_usage = 1;
			break;
		}
	}

	if (show_usage)
		usage(search.argmode);

	/* Postprocess options. */

	switch (outmode) {
	case OUTMODE_DEF:
		switch (search.argmode) {
		case ARG_FILE:
			outmode = OUTMODE_ALL;
			outst.use_pager = 0;
			break;
		case ARG_NAME:
			outmode = OUTMODE_ONE;
			break;
		default:
			outmode = OUTMODE_LST;
			break;
		}
		break;
	case OUTMODE_FLN:
		if (search.argmode == ARG_FILE)
			outmode = OUTMODE_ALL;
		break;
	case OUTMODE_ALL:
		break;
	case OUTMODE_LST:
	case OUTMODE_ONE:
		abort();
	}

	if (oarg != NULL) {
		if (outmode == OUTMODE_LST)
			search.outkey = oarg;
		else {
			while (oarg != NULL) {
				if (manconf_output(&conf.output,
				    strsep(&oarg, ","), 0) == -1)
					return mandoc_msg_getrc();
			}
		}
	}

	if (outst.outtype != OUTT_TREE || conf.output.noval == 0)
		options |= MPARSE_VALIDATE;

	if (outmode == OUTMODE_FLN ||
	    outmode == OUTMODE_LST ||
	    (conf.output.outfilename == NULL &&
	     conf.output.tagfilename == NULL &&
	     isatty(STDOUT_FILENO) == 0))
		outst.use_pager = 0;

	if (outst.use_pager &&
	    (conf.output.width == 0 || conf.output.indent == 0) &&
	    ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) != -1 &&
	    ws.ws_col > 1) {
		if (conf.output.width == 0 && ws.ws_col < 79)
			conf.output.width = ws.ws_col - 1;
		if (conf.output.indent == 0 && ws.ws_col < 66)
			conf.output.indent = 3;
	}

#if HAVE_PLEDGE
	if (outst.use_pager == 0)
		c = pledge("stdio rpath", NULL);
	else if (conf.output.outfilename != NULL ||
	    conf.output.tagfilename != NULL)
		c = pledge("stdio rpath wpath cpath", NULL);
	else
		c = pledge("stdio rpath tmppath tty proc exec", NULL);
	if (c == -1) {
		mandoc_msg(MANDOCERR_PLEDGE, 0, 0, "%s", strerror(errno));
		return mandoc_msg_getrc();
	}
#endif

	/* Parse arguments. */

	if (argc > 0) {
		argc -= optind;
		argv += optind;
	}

	/*
	 * Quirks for help(1) and man(1),
	 * in particular for a section argument without -s.
	 */

	if (search.argmode == ARG_NAME) {
		if (*progname == 'h') {
			if (argc == 0) {
				argv = help_argv;
				argc = 1;
			}
		} else if (argc > 1 &&
		    ((uc = (unsigned char *)argv[0]) != NULL) &&
		    ((isdigit(uc[0]) && (uc[1] == '\0' ||
		      isalpha(uc[1]))) ||
		     (uc[0] == 'n' && uc[1] == '\0'))) {
			search.sec = (char *)uc;
			argv++;
			argc--;
		}
		if (search.arch == NULL)
			search.arch = getenv("MACHINE");
#ifdef MACHINE
		if (search.arch == NULL)
			search.arch = MACHINE;
#endif
		if (outmode == OUTMODE_ONE)
			search.firstmatch = 1;
	}

	/*
	 * Use the first argument for -O tag in addition to
	 * using it as a search term for man(1) or apropos(1).
	 */

	if (conf.output.tag != NULL && *conf.output.tag == '\0') {
		tagarg = argc > 0 && search.argmode == ARG_EXPR ?
		    strchr(*argv, '=') : NULL;
		conf.output.tag = tagarg == NULL ? *argv : tagarg + 1;
	}

	/* Read the configuration file. */

	if (search.argmode != ARG_FILE ||
	    mandoc_msg_getmin() == MANDOCERR_STYLE)
		manconf_parse(&conf, conf_file, defpaths, auxpaths);

	/* man(1): Resolve each name individually. */

	if (search.argmode == ARG_NAME) {
		if (argc < 1) {
			if (outmode != OUTMODE_FLN)
				usage(ARG_NAME);
			if (conf.manpath.sz == 0) {
				warnx("The manpath is empty.");
				mandoc_msg_setrc(MANDOCLEVEL_BADARG);
			} else {
				for (i = 0; i + 1 < conf.manpath.sz; i++)
					printf("%s:", conf.manpath.paths[i]);
				printf("%s\n", conf.manpath.paths[i]);
			}
			manconf_free(&conf);
			return (int)mandoc_msg_getrc();
		}
		for (res = NULL, ressz = 0; argc > 0; argc--, argv++) {
			(void)mansearch(&search, &conf.manpath,
			    1, argv, &resn, &resnsz);
			if (resnsz == 0)
				(void)fs_search(&search, &conf.manpath,
				    *argv, &resn, &resnsz);
			if (resnsz == 0 && strchr(*argv, '/') == NULL) {
				if (search.arch != NULL &&
				    arch_valid(search.arch, OSENUM) == 0)
					warnx("Unknown architecture \"%s\".",
					    search.arch);
				else if (search.sec != NULL)
					warnx("No entry for %s in "
					    "section %s of the manual.",
					    *argv, search.sec);
				else
					warnx("No entry for %s in "
					    "the manual.", *argv);
				mandoc_msg_setrc(MANDOCLEVEL_BADARG);
				continue;
			}
			if (resnsz == 0) {
				if (access(*argv, R_OK) == -1) {
					mandoc_msg_setinfilename(*argv);
					mandoc_msg(MANDOCERR_BADARG_BAD,
					    0, 0, "%s", strerror(errno));
					mandoc_msg_setinfilename(NULL);
					continue;
				}
				resnsz = 1;
				resn = mandoc_calloc(resnsz, sizeof(*res));
				resn->file = mandoc_strdup(*argv);
				resn->ipath = SIZE_MAX;
				resn->form = FORM_SRC;
			}
			if (outmode != OUTMODE_ONE || resnsz == 1) {
				res = mandoc_reallocarray(res,
				    ressz + resnsz, sizeof(*res));
				memcpy(res + ressz, resn,
				    sizeof(*resn) * resnsz);
				ressz += resnsz;
				continue;
			}

			/* Search for the best section. */

			best_prio = 40;
			for (ib = i = 0; i < resnsz; i++) {
				sec = resn[i].file;
				sec += strcspn(sec, "123456789");
				if (sec[0] == '\0')
					continue; /* No section at all. */
				prio = sec_prios[sec[0] - '1'];
				if (search.sec != NULL) {
					ssz = strlen(search.sec);
					if (strncmp(sec, search.sec, ssz) == 0)
						sec += ssz;
				} else
					sec++; /* Prefer without suffix. */
				if (*sec != '/')
					prio += 10; /* Wrong dir name. */
				if (search.sec != NULL) {
					ep = strchr(sec, '\0');
					if (ep - sec > 3 &&
					    strncmp(ep - 3, ".gz", 3) == 0)
						ep -= 3;
					if ((size_t)(ep - sec) < ssz + 3 ||
					    strncmp(ep - ssz, search.sec,
					     ssz) != 0)      /* Wrong file */
						prio += 20;  /* extension. */
				}
				if (prio >= best_prio)
					continue;
				best_prio = prio;
				ib = i;
			}
			res = mandoc_reallocarray(res, ressz + 1,
			    sizeof(*res));
			memcpy(res + ressz++, resn + ib, sizeof(*resn));
		}

	/* apropos(1), whatis(1): Process the full search expression. */

	} else if (search.argmode != ARG_FILE) {
		if (mansearch(&search, &conf.manpath,
		    argc, argv, &res, &ressz) == 0)
			usage(search.argmode);

		if (ressz == 0) {
			warnx("nothing appropriate");
			mandoc_msg_setrc(MANDOCLEVEL_BADARG);
			goto out;
		}

	/* mandoc(1): Take command line arguments as file names. */

	} else {
		ressz = argc > 0 ? argc : 1;
		res = mandoc_calloc(ressz, sizeof(*res));
		for (i = 0; i < ressz; i++) {
			if (argc > 0)
				res[i].file = mandoc_strdup(argv[i]);
			res[i].ipath = SIZE_MAX;
			res[i].form = FORM_SRC;
		}
	}

	switch (outmode) {
	case OUTMODE_FLN:
		for (i = 0; i < ressz; i++)
			puts(res[i].file);
		goto out;
	case OUTMODE_LST:
		for (i = 0; i < ressz; i++)
			printf("%s - %s\n", res[i].names,
			    res[i].output == NULL ? "" :
			    res[i].output);
		goto out;
	default:
		break;
	}

	if (search.argmode == ARG_FILE && auxpaths != NULL) {
		if (strcmp(auxpaths, "doc") == 0)
			options |= MPARSE_MDOC;
		else if (strcmp(auxpaths, "an") == 0)
			options |= MPARSE_MAN;
	}

	mchars_alloc();
	mp = mparse_alloc(options, os_e, os_s);

	/*
	 * Remember the original working directory, if possible.
	 * This will be needed if some names on the command line
	 * are page names and some are relative file names.
	 * Do not error out if the current directory is not
	 * readable: Maybe it won't be needed after all.
	 */
	startdir = open(".", O_RDONLY | O_DIRECTORY);
	for (i = 0; i < ressz; i++) {
		process_onefile(mp, res + i, startdir, &outst, &conf);
		if (outst.wstop && mandoc_msg_getrc() != MANDOCLEVEL_OK)
			break;
	}
	if (startdir != -1) {
		(void)fchdir(startdir);
		close(startdir);
	}
	if (conf.output.tag != NULL && conf.output.tag_found == 0) {
		mandoc_msg(MANDOCERR_TAG, 0, 0, "%s", conf.output.tag);
		conf.output.tag = NULL;
	}
	if (outst.outdata != NULL) {
		switch (outst.outtype) {
		case OUTT_HTML:
			html_free(outst.outdata);
			break;
		case OUTT_UTF8:
		case OUTT_LOCALE:
		case OUTT_ASCII:
			ascii_free(outst.outdata);
			break;
		case OUTT_PDF:
		case OUTT_PS:
			pspdf_free(outst.outdata);
			break;
		default:
			break;
		}
	}
	mandoc_xr_free();
	mparse_free(mp);
	mchars_free();

out:
	mansearch_free(res, ressz);
	if (search.argmode != ARG_FILE)
		manconf_free(&conf);

	if (outst.tag_files != NULL) {
		if (term_tag_close() != -1 &&
		    conf.output.outfilename == NULL &&
		    conf.output.tagfilename == NULL)
			run_pager(&outst, conf.output.tag);
		term_tag_unlink();
	} else if (outst.had_output && outst.outtype != OUTT_LINT)
		mandoc_msg_summary();

	return (int)mandoc_msg_getrc();
}

static void
usage(enum argmode argmode)
{
	switch (argmode) {
	case ARG_FILE:
		fputs("usage: mandoc [-ac] [-I os=name] "
		    "[-K encoding] [-mdoc | -man] [-O options]\n"
		    "\t      [-T output] [-W level] [file ...]\n", stderr);
		break;
	case ARG_NAME:
		fputs("usage: man [-acfhklw] [-C file] [-M path] "
		    "[-m path] [-S subsection]\n"
		    "\t   [[-s] section] name ...\n", stderr);
		break;
	case ARG_WORD:
		fputs("usage: whatis [-afk] [-C file] "
		    "[-M path] [-m path] [-O outkey] [-S arch]\n"
		    "\t      [-s section] name ...\n", stderr);
		break;
	case ARG_EXPR:
		fputs("usage: apropos [-afk] [-C file] "
		    "[-M path] [-m path] [-O outkey] [-S arch]\n"
		    "\t       [-s section] expression ...\n", stderr);
		break;
	}
	exit((int)MANDOCLEVEL_BADARG);
}

static void
glob_esc(char **dst, const char *src, const char *suffix)
{
	while (*src != '\0') {
		if (strchr("*?[", *src) != NULL)
			*(*dst)++ = '\\';
		*(*dst)++ = *src++;
	}
	while (*suffix != '\0')
		*(*dst)++ = *suffix++;
}

static void
fs_append(char **file, size_t filesz, int copy, size_t ipath,
    const char *sec, enum form form, struct manpage **res, size_t *ressz)
{
	struct manpage	*page;

	*res = mandoc_reallocarray(*res, *ressz + filesz, sizeof(**res));
	page = *res + *ressz;
	*ressz += filesz;
	for (;;) {
		page->file = copy ? mandoc_strdup(*file) : *file;
		page->names = NULL;
		page->output = NULL;
		page->bits = NAME_FILE & NAME_MASK;
		page->ipath = ipath;
		page->sec = (*sec >= '1' && *sec <= '9') ? *sec - '1' + 1 : 10;
		page->form = form;
		if (--filesz == 0)
			break;
		file++;
		page++;
	}
}

static int
fs_lookup(const struct manpaths *paths, size_t ipath,
	const char *sec, const char *arch, const char *name,
	struct manpage **res, size_t *ressz)
{
	struct stat	 sb;
	glob_t		 globinfo;
	char		*file, *cp, secnum[2];
	int		 globres;
	enum form	 form;

	const char *const slman = "/man";
	const char *const slash = "/";
	const char *const sglob = ".[01-9]*";
	const char *const dot   = ".";
	const char *const aster = "*";

	memset(&globinfo, 0, sizeof(globinfo));
	form = FORM_SRC;

	mandoc_asprintf(&file, "%s/man%s/%s.%s",
	    paths->paths[ipath], sec, name, sec);
	if (stat(file, &sb) != -1)
		goto found;
	free(file);

	mandoc_asprintf(&file, "%s/cat%s/%s.0",
	    paths->paths[ipath], sec, name);
	if (stat(file, &sb) != -1) {
		form = FORM_CAT;
		goto found;
	}
	free(file);

	if (arch != NULL) {
		mandoc_asprintf(&file, "%s/man%s/%s/%s.%s",
		    paths->paths[ipath], sec, arch, name, sec);
		if (stat(file, &sb) != -1)
			goto found;
		free(file);
	}

	cp = file = mandoc_malloc(strlen(paths->paths[ipath]) * 2 +
	    strlen(slman) + strlen(sec) * 2 + strlen(slash) +
	    strlen(name) * 2 + strlen(sglob) + 1);
	glob_esc(&cp, paths->paths[ipath], slman);
	glob_esc(&cp, sec, slash);
	glob_esc(&cp, name, sglob);
	*cp = '\0';
	globres = glob(file, 0, NULL, &globinfo);
	if (globres != 0 && globres != GLOB_NOMATCH)
		mandoc_msg(MANDOCERR_GLOB, 0, 0,
		    "%s: %s", file, strerror(errno));
	free(file);
	file = NULL;
	if (globres == 0)
		goto found;
	globfree(&globinfo);

	if (sec[1] != '\0' && *ressz == 0) {
		secnum[0] = sec[0];
		secnum[1] = '\0';
		cp = file = mandoc_malloc(strlen(paths->paths[ipath]) * 2 +
		    strlen(slman) + strlen(secnum) * 2 + strlen(slash) +
		    strlen(name) * 2 + strlen(dot) +
		    strlen(sec) * 2 + strlen(aster) + 1);
		glob_esc(&cp, paths->paths[ipath], slman);
		glob_esc(&cp, secnum, slash);
		glob_esc(&cp, name, dot);
		glob_esc(&cp, sec, aster);
		*cp = '\0';
		globres = glob(file, 0, NULL, &globinfo);
		if (globres != 0 && globres != GLOB_NOMATCH)
			mandoc_msg(MANDOCERR_GLOB, 0, 0,
			    "%s: %s", file, strerror(errno));
		free(file);
		file = NULL;
		if (globres == 0)
			goto found;
		globfree(&globinfo);
	}

	if (res != NULL || ipath + 1 != paths->sz)
		return -1;

	mandoc_asprintf(&file, "%s.%s", name, sec);
	globres = stat(file, &sb);
	free(file);
	return globres;

found:
	warnx("outdated mandoc.db lacks %s(%s) entry, run %s %s",
	    name, sec, BINM_MAKEWHATIS, paths->paths[ipath]);
	if (res == NULL)
		free(file);
	else if (file == NULL)
		fs_append(globinfo.gl_pathv, globinfo.gl_pathc, 1,
		    ipath, sec, form, res, ressz);
	else
		fs_append(&file, 1, 0, ipath, sec, form, res, ressz);
	globfree(&globinfo);
	return 0;
}

static int
fs_search(const struct mansearch *cfg, const struct manpaths *paths,
	const char *name, struct manpage **res, size_t *ressz)
{
	const char *const sections[] =
	    {"1", "8", "6", "2", "3", "5", "7", "4", "9", "3p"};
	const size_t nsec = sizeof(sections)/sizeof(sections[0]);

	size_t		 ipath, isec;

	assert(cfg->argmode == ARG_NAME);
	if (res != NULL)
		*res = NULL;
	*ressz = 0;
	for (ipath = 0; ipath < paths->sz; ipath++) {
		if (cfg->sec != NULL) {
			if (fs_lookup(paths, ipath, cfg->sec, cfg->arch,
			    name, res, ressz) != -1 && cfg->firstmatch)
				return 0;
		} else {
			for (isec = 0; isec < nsec; isec++)
				if (fs_lookup(paths, ipath, sections[isec],
				    cfg->arch, name, res, ressz) != -1 &&
				    cfg->firstmatch)
					return 0;
		}
	}
	return -1;
}

static void
process_onefile(struct mparse *mp, struct manpage *resp, int startdir,
    struct outstate *outst, struct manconf *conf)
{
	int	 fd;

	/*
	 * Changing directories is not needed in ARG_FILE mode.
	 * Do it on a best-effort basis.  Even in case of
	 * failure, some functionality may still work.
	 */
	if (resp->ipath != SIZE_MAX)
		(void)chdir(conf->manpath.paths[resp->ipath]);
	else if (startdir != -1)
		(void)fchdir(startdir);

	mandoc_msg_setinfilename(resp->file);
	if (resp->file != NULL) {
		if ((fd = mparse_open(mp, resp->file)) == -1) {
			mandoc_msg(resp->ipath == SIZE_MAX ?
			    MANDOCERR_BADARG_BAD : MANDOCERR_OPEN,
			    0, 0, "%s", strerror(errno));
			mandoc_msg_setinfilename(NULL);
			return;
		}
	} else
		fd = STDIN_FILENO;

	if (outst->use_pager) {
		outst->use_pager = 0;
		outst->tag_files = term_tag_init(conf->output.outfilename,
		    outst->outtype == OUTT_HTML ? ".html" : "",
		    conf->output.tagfilename);
#if HAVE_PLEDGE
		if ((conf->output.outfilename != NULL ||
		     conf->output.tagfilename != NULL) &&
		    pledge("stdio rpath cpath", NULL) == -1) {
			mandoc_msg(MANDOCERR_PLEDGE, 0, 0,
			    "%s", strerror(errno));
			exit(mandoc_msg_getrc());
		}
#endif
	}
	if (outst->had_output && outst->outtype <= OUTT_UTF8) {
		if (outst->outdata == NULL)
			outdata_alloc(outst, &conf->output);
		terminal_sepline(outst->outdata);
	}

	if (resp->form == FORM_SRC)
		parse(mp, fd, resp->file, outst, conf);
	else {
		passthrough(fd, conf->output.synopsisonly);
		outst->had_output = 1;
	}

	if (ferror(stdout)) {
		if (outst->tag_files != NULL) {
			mandoc_msg(MANDOCERR_WRITE, 0, 0, "%s: %s",
			    outst->tag_files->ofn, strerror(errno));
			term_tag_unlink();
			outst->tag_files = NULL;
		} else
			mandoc_msg(MANDOCERR_WRITE, 0, 0, "%s",
			    strerror(errno));
	}
	mandoc_msg_setinfilename(NULL);
}

static void
parse(struct mparse *mp, int fd, const char *file,
    struct outstate *outst, struct manconf *conf)
{
	static struct manpaths	 basepaths;
	static int		 previous;
	struct roff_meta	*meta;

	assert(fd >= 0);
	if (file == NULL)
		file = "<stdin>";

	if (previous)
		mparse_reset(mp);
	else
		previous = 1;

	mparse_readfd(mp, fd, file);
	if (fd != STDIN_FILENO)
		close(fd);

	/*
	 * With -Wstop and warnings or errors of at least the requested
	 * level, do not produce output.
	 */

	if (outst->wstop && mandoc_msg_getrc() != MANDOCLEVEL_OK)
		return;

	if (outst->outdata == NULL)
		outdata_alloc(outst, &conf->output);
	else if (outst->outtype == OUTT_HTML)
		html_reset(outst->outdata);

	mandoc_xr_reset();
	meta = mparse_result(mp);

	/* Execute the out device, if it exists. */

	outst->had_output = 1;
	if (meta->macroset == MACROSET_MDOC) {
		switch (outst->outtype) {
		case OUTT_HTML:
			html_mdoc(outst->outdata, meta);
			break;
		case OUTT_TREE:
			tree_mdoc(outst->outdata, meta);
			break;
		case OUTT_MAN:
			man_mdoc(outst->outdata, meta);
			break;
		case OUTT_PDF:
		case OUTT_ASCII:
		case OUTT_UTF8:
		case OUTT_LOCALE:
		case OUTT_PS:
			terminal_mdoc(outst->outdata, meta);
			break;
		case OUTT_MARKDOWN:
			markdown_mdoc(outst->outdata, meta);
			break;
		default:
			break;
		}
	}
	if (meta->macroset == MACROSET_MAN) {
		switch (outst->outtype) {
		case OUTT_HTML:
			html_man(outst->outdata, meta);
			break;
		case OUTT_TREE:
			tree_man(outst->outdata, meta);
			break;
		case OUTT_MAN:
			mparse_copy(mp);
			break;
		case OUTT_PDF:
		case OUTT_ASCII:
		case OUTT_UTF8:
		case OUTT_LOCALE:
		case OUTT_PS:
			terminal_man(outst->outdata, meta);
			break;
		case OUTT_MARKDOWN:
			mandoc_msg(MANDOCERR_MAN_TMARKDOWN, 0, 0, NULL);
			break;
		default:
			break;
		}
	}
	if (conf->output.tag != NULL && conf->output.tag_found == 0 &&
	    tag_exists(conf->output.tag))
		conf->output.tag_found = 1;

	if (mandoc_msg_getmin() < MANDOCERR_STYLE) {
		if (basepaths.sz == 0)
			manpath_base(&basepaths);
		check_xr(&basepaths);
	} else if (mandoc_msg_getmin() < MANDOCERR_WARNING)
		check_xr(&conf->manpath);
}

static void
check_xr(struct manpaths *paths)
{
	struct mansearch	 search;
	struct mandoc_xr	*xr;
	size_t			 sz;

	for (xr = mandoc_xr_get(); xr != NULL; xr = xr->next) {
		if (xr->line == -1)
			continue;
		search.arch = NULL;
		search.sec = xr->sec;
		search.outkey = NULL;
		search.argmode = ARG_NAME;
		search.firstmatch = 1;
		if (mansearch(&search, paths, 1, &xr->name, NULL, &sz))
			continue;
		if (fs_search(&search, paths, xr->name, NULL, &sz) != -1)
			continue;
		if (xr->count == 1)
			mandoc_msg(MANDOCERR_XR_BAD, xr->line,
			    xr->pos + 1, "Xr %s %s", xr->name, xr->sec);
		else
			mandoc_msg(MANDOCERR_XR_BAD, xr->line,
			    xr->pos + 1, "Xr %s %s (%d times)",
			    xr->name, xr->sec, xr->count);
	}
}

static void
outdata_alloc(struct outstate *outst, struct manoutput *outconf)
{
	switch (outst->outtype) {
	case OUTT_HTML:
		outst->outdata = html_alloc(outconf);
		break;
	case OUTT_UTF8:
		outst->outdata = utf8_alloc(outconf);
		break;
	case OUTT_LOCALE:
		outst->outdata = locale_alloc(outconf);
		break;
	case OUTT_ASCII:
		outst->outdata = ascii_alloc(outconf);
		break;
	case OUTT_PDF:
		outst->outdata = pdf_alloc(outconf);
		break;
	case OUTT_PS:
		outst->outdata = ps_alloc(outconf);
		break;
	default:
		break;
	}
}

static void
passthrough(int fd, int synopsis_only)
{
	const char	 synb[] = "S\bSY\bYN\bNO\bOP\bPS\bSI\bIS\bS";
	const char	 synr[] = "SYNOPSIS";

	FILE		*stream;
	char		*line, *cp;
	size_t		 linesz;
	ssize_t		 len, written;
	int		 lno, print;

	stream = NULL;
	line = NULL;
	linesz = 0;

	if (fflush(stdout) == EOF) {
		mandoc_msg(MANDOCERR_FFLUSH, 0, 0, "%s", strerror(errno));
		goto done;
	}
	if ((stream = fdopen(fd, "r")) == NULL) {
		close(fd);
		mandoc_msg(MANDOCERR_FDOPEN, 0, 0, "%s", strerror(errno));
		goto done;
	}

	lno = print = 0;
	while ((len = getline(&line, &linesz, stream)) != -1) {
		lno++;
		cp = line;
		if (synopsis_only) {
			if (print) {
				if ( ! isspace((unsigned char)*cp))
					goto done;
				while (isspace((unsigned char)*cp)) {
					cp++;
					len--;
				}
			} else {
				if (strcmp(cp, synb) == 0 ||
				    strcmp(cp, synr) == 0)
					print = 1;
				continue;
			}
		}
		for (; len > 0; len -= written) {
			if ((written = write(STDOUT_FILENO, cp, len)) == -1) {
				mandoc_msg(MANDOCERR_WRITE, 0, 0,
				    "%s", strerror(errno));
				goto done;
			}
		}
	}
	if (ferror(stream))
		mandoc_msg(MANDOCERR_GETLINE, lno, 0, "%s", strerror(errno));

done:
	free(line);
	if (stream != NULL)
		fclose(stream);
}

static int
woptions(char *arg, enum mandoc_os *os_e, int *wstop)
{
	char		*v, *o;
	const char	*toks[11];

	toks[0] = "stop";
	toks[1] = "all";
	toks[2] = "base";
	toks[3] = "style";
	toks[4] = "warning";
	toks[5] = "error";
	toks[6] = "unsupp";
	toks[7] = "fatal";
	toks[8] = "openbsd";
	toks[9] = "netbsd";
	toks[10] = NULL;

	while (*arg) {
		o = arg;
		switch (getsubopt(&arg, (char * const *)toks, &v)) {
		case 0:
			*wstop = 1;
			break;
		case 1:
		case 2:
			mandoc_msg_setmin(MANDOCERR_BASE);
			break;
		case 3:
			mandoc_msg_setmin(MANDOCERR_STYLE);
			break;
		case 4:
			mandoc_msg_setmin(MANDOCERR_WARNING);
			break;
		case 5:
			mandoc_msg_setmin(MANDOCERR_ERROR);
			break;
		case 6:
			mandoc_msg_setmin(MANDOCERR_UNSUPP);
			break;
		case 7:
			mandoc_msg_setmin(MANDOCERR_BADARG);
			break;
		case 8:
			mandoc_msg_setmin(MANDOCERR_BASE);
			*os_e = MANDOC_OS_OPENBSD;
			break;
		case 9:
			mandoc_msg_setmin(MANDOCERR_BASE);
			*os_e = MANDOC_OS_NETBSD;
			break;
		default:
			mandoc_msg(MANDOCERR_BADARG_BAD, 0, 0, "-W %s", o);
			return -1;
		}
	}
	return 0;
}

/*
 * Wait until moved to the foreground,
 * then fork the pager and wait for the user to close it.
 */
static void
run_pager(struct outstate *outst, char *tag_target)
{
	int	 signum, status;
	pid_t	 man_pgid, tc_pgid;
	pid_t	 pager_pid, wait_pid;

	man_pgid = getpgid(0);
	outst->tag_files->tcpgid =
	    man_pgid == getpid() ? getpgid(getppid()) : man_pgid;
	pager_pid = 0;
	signum = SIGSTOP;

	for (;;) {
		/* Stop here until moved to the foreground. */

		tc_pgid = tcgetpgrp(STDOUT_FILENO);
		if (tc_pgid != man_pgid) {
			if (tc_pgid == pager_pid) {
				(void)tcsetpgrp(STDOUT_FILENO, man_pgid);
				if (signum == SIGTTIN)
					continue;
			} else
				outst->tag_files->tcpgid = tc_pgid;
			kill(0, signum);
			continue;
		}

		/* Once in the foreground, activate the pager. */

		if (pager_pid) {
			(void)tcsetpgrp(STDOUT_FILENO, pager_pid);
			kill(pager_pid, SIGCONT);
		} else
			pager_pid = spawn_pager(outst, tag_target);

		/* Wait for the pager to stop or exit. */

		while ((wait_pid = waitpid(pager_pid, &status,
		    WUNTRACED)) == -1 && errno == EINTR)
			continue;

		if (wait_pid == -1) {
			mandoc_msg(MANDOCERR_WAIT, 0, 0,
			    "%s", strerror(errno));
			break;
		}
		if (!WIFSTOPPED(status))
			break;

		signum = WSTOPSIG(status);
	}
}

static pid_t
spawn_pager(struct outstate *outst, char *tag_target)
{
	const struct timespec timeout = { 0, 100000000 };  /* 0.1s */
#define MAX_PAGER_ARGS 16
	char		*argv[MAX_PAGER_ARGS];
	const char	*pager;
	char		*cp;
#if HAVE_LESS_T
	size_t		 cmdlen;
#endif
	int		 argc, use_ofn;
	pid_t		 pager_pid;

	assert(outst->tag_files->ofd == -1);
	assert(outst->tag_files->tfs == NULL);

	pager = getenv("MANPAGER");
	if (pager == NULL || *pager == '\0')
		pager = getenv("PAGER");
	if (pager == NULL || *pager == '\0')
		pager = BINM_PAGER;
	cp = mandoc_strdup(pager);

	/*
	 * Parse the pager command into words.
	 * Intentionally do not do anything fancy here.
	 */

	argc = 0;
	while (argc + 5 < MAX_PAGER_ARGS) {
		argv[argc++] = cp;
		cp = strchr(cp, ' ');
		if (cp == NULL)
			break;
		*cp++ = '\0';
		while (*cp == ' ')
			cp++;
		if (*cp == '\0')
			break;
	}

	/* For less(1), use the tag file. */

	use_ofn = 1;
#if HAVE_LESS_T
	if (*outst->tag_files->tfn != '\0' &&
	    (cmdlen = strlen(argv[0])) >= 4) {
		cp = argv[0] + cmdlen - 4;
		if (strcmp(cp, "less") == 0) {
			argv[argc++] = mandoc_strdup("-T");
			argv[argc++] = outst->tag_files->tfn;
			if (tag_target != NULL) {
				argv[argc++] = mandoc_strdup("-t");
				argv[argc++] = tag_target;
				use_ofn = 0;
			}
		}
	}
#endif
	if (use_ofn) {
		if (outst->outtype == OUTT_HTML && tag_target != NULL)
			mandoc_asprintf(&argv[argc], "file://%s#%s",
			    outst->tag_files->ofn, tag_target);
		else
			argv[argc] = outst->tag_files->ofn;
		argc++;
	}
	argv[argc] = NULL;

	switch (pager_pid = fork()) {
	case -1:
		mandoc_msg(MANDOCERR_FORK, 0, 0, "%s", strerror(errno));
		exit(mandoc_msg_getrc());
	case 0:
		break;
	default:
		(void)setpgid(pager_pid, 0);
		(void)tcsetpgrp(STDOUT_FILENO, pager_pid);
#if HAVE_PLEDGE
		if (pledge("stdio rpath tmppath tty proc", NULL) == -1) {
			mandoc_msg(MANDOCERR_PLEDGE, 0, 0,
			    "%s", strerror(errno));
			exit(mandoc_msg_getrc());
		}
#endif
		outst->tag_files->pager_pid = pager_pid;
		return pager_pid;
	}

	/*
	 * The child process becomes the pager.
	 * Do not start it before controlling the terminal.
	 */

	while (tcgetpgrp(STDOUT_FILENO) != getpid())
		nanosleep(&timeout, NULL);

	execvp(argv[0], argv);
	mandoc_msg(MANDOCERR_EXEC, 0, 0, "%s: %s", argv[0], strerror(errno));
	_exit(mandoc_msg_getrc());
}
