/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Edward Sze-Tyan Wang.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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


#include <sys/capsicum.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <capsicum_helpers.h>
#include <err.h>
#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <libutil.h>

#include <libcasper.h>
#include <casper/cap_fileargs.h>

#include "extern.h"

int Fflag, fflag, qflag, rflag, rval, no_files, vflag;
fileargs_t *fa;

static void obsolete(char **);
static void usage(void) __dead2;

static const struct option long_opts[] =
{
	{"blocks",	required_argument,	NULL, 'b'},
	{"bytes",	required_argument,	NULL, 'c'},
	{"lines",	required_argument,	NULL, 'n'},
	{"quiet",	no_argument,		NULL, 'q'},
	{"silent",	no_argument,		NULL, 'q'},
	{"verbose",	no_argument,		NULL, 'v'},
	{NULL,		no_argument,		NULL, 0}
};

int
main(int argc, char *argv[])
{
	struct stat sb;
	const char *fn;
	FILE *fp;
	off_t off;
	enum STYLE style;
	int ch, first;
	file_info_t file, *filep, *files;
	cap_rights_t rights;

	/*
	 * Tail's options are weird.  First, -n10 is the same as -n-10, not
	 * -n+10.  Second, the number options are 1 based and not offsets,
	 * so -n+1 is the first line, and -c-1 is the last byte.  Third, the
	 * number options for the -r option specify the number of things that
	 * get displayed, not the starting point in the file.  The one major
	 * incompatibility in this version as compared to historical versions
	 * is that the 'r' option couldn't be modified by the -lbc options,
	 * i.e. it was always done in lines.  This version treats -rc as a
	 * number of characters in reverse order.  Finally, the default for
	 * -r is the entire file, not 10 lines.
	 */
#define	ARG(units, forward, backward) {					\
	int64_t num;							\
	if (style)							\
		usage();						\
	if (expand_number(optarg, &num))				\
		err(1, "illegal offset -- %s", optarg);			\
	if (num > INT64_MAX / units || num < INT64_MIN / units)		\
		errx(1, "illegal offset -- %s", optarg);		\
	off = num * units;						\
	switch (optarg[0]) {						\
	case '+':							\
		if (off != 0)						\
			off -= (units);					\
		style = (forward);					\
		break;							\
	case '-':							\
		off = -off;						\
		/* FALLTHROUGH */					\
	default:							\
		style = (backward);					\
		break;							\
	}								\
}

	obsolete(argv);
	style = NOTSET;
	off = 0;
	while ((ch = getopt_long(argc, argv, "+Fb:c:fn:qrv", long_opts, NULL)) !=
	    -1)
		switch (ch) {
		case 'F':	/* -F is superset of (and implies) -f */
			Fflag = fflag = 1;
			break;
		case 'b':
			ARG(512, FBYTES, RBYTES);
			break;
		case 'c':
			ARG(1, FBYTES, RBYTES);
			break;
		case 'f':
			fflag = 1;
			break;
		case 'n':
			ARG(1, FLINES, RLINES);
			break;
		case 'q':
			qflag = 1;
			vflag = 0;
			break;
		case 'r':
			rflag = 1;
			break;
		case 'v':
			vflag = 1;
			qflag = 0;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	no_files = argc ? argc : 1;

	cap_rights_init(&rights, CAP_FSTAT, CAP_FSTATFS, CAP_FCNTL,
	    CAP_MMAP_R);
	if (fflag)
		cap_rights_set(&rights, CAP_EVENT);
	if (caph_rights_limit(STDIN_FILENO, &rights) < 0 ||
	    caph_limit_stderr() < 0 || caph_limit_stdout() < 0)
		err(1, "unable to limit stdio rights");

	fa = fileargs_init(argc, argv, O_RDONLY, 0, &rights, FA_OPEN);
	if (fa == NULL)
		err(1, "unable to init casper");

	caph_cache_catpages();
	if (caph_enter_casper() < 0)
		err(1, "unable to enter capability mode");

	/*
	 * If displaying in reverse, don't permit follow option, and convert
	 * style values.
	 */
	if (rflag) {
		if (fflag)
			usage();
		if (style == FBYTES)
			style = RBYTES;
		else if (style == FLINES)
			style = RLINES;
	}

	/*
	 * If style not specified, the default is the whole file for -r, and
	 * the last 10 lines if not -r.
	 */
	if (style == NOTSET) {
		if (rflag) {
			off = 0;
			style = REVERSE;
		} else {
			off = 10;
			style = RLINES;
		}
	}

	if (*argv && fflag) {
		files = malloc(no_files * sizeof(struct file_info));
		if (files == NULL)
			err(1, "failed to allocate memory for file descriptors");

		for (filep = files; (fn = *argv++); filep++) {
			filep->file_name = fn;
			filep->fp = fileargs_fopen(fa, filep->file_name, "r");
			if (filep->fp == NULL ||
			    fstat(fileno(filep->fp), &filep->st)) {
				if (filep->fp != NULL) {
					fclose(filep->fp);
					filep->fp = NULL;
				}
				if (!Fflag || errno != ENOENT)
					ierr(filep->file_name);
			}
		}
		follow(files, style, off);
		free(files);
	} else if (*argv) {
		for (first = 1; (fn = *argv++);) {
			if ((fp = fileargs_fopen(fa, fn, "r")) == NULL ||
			    fstat(fileno(fp), &sb)) {
				ierr(fn);
				continue;
			}
			if (vflag || (qflag == 0 && argc > 1)) {
				printfn(fn, !first);
				first = 0;
			}

			if (rflag)
				reverse(fp, fn, style, off, &sb);
			else
				forward(fp, fn, style, off, &sb);
		}
	} else {
		fn = "stdin";

		if (fstat(fileno(stdin), &sb)) {
			ierr(fn);
			exit(1);
		}

		/*
		 * Determine if input is a pipe.  4.4BSD will set the SOCKET
		 * bit in the st_mode field for pipes.  Fix this then.
		 */
		if (lseek(fileno(stdin), (off_t)0, SEEK_CUR) == -1 &&
		    errno == ESPIPE) {
			errno = 0;
			fflag = 0;		/* POSIX.2 requires this. */
		}

		if (rflag) {
			reverse(stdin, fn, style, off, &sb);
		} else if (fflag) {
			file.file_name = fn;
			file.fp = stdin;
			file.st = sb;
			follow(&file, style, off);
		} else {
			forward(stdin, fn, style, off, &sb);
		}
	}
	fileargs_free(fa);
	exit(rval);
}

/*
 * Convert the obsolete argument form into something that getopt can handle.
 * This means that anything of the form [+-][0-9][0-9]*[lbc][Ffr] that isn't
 * the option argument for a -b, -c or -n option gets converted.
 */
static void
obsolete(char *argv[])
{
	char *ap, *p, *t;
	size_t len;
	char *start;

	while ((ap = *++argv)) {
		/* Return if "--" or not an option of any form. */
		if (ap[0] != '-') {
			if (ap[0] != '+')
				return;
		} else if (ap[1] == '-')
			return;

		switch(*++ap) {
		/* Old-style option. */
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':

			/* Malloc space for dash, new option and argument. */
			len = strlen(*argv);
			if ((start = p = malloc(len + 3)) == NULL)
				err(1, "failed to allocate memory");
			*p++ = '-';

			/*
			 * Go to the end of the option argument.  Save off any
			 * trailing options (-3lf) and translate any trailing
			 * output style characters.
			 */
			t = *argv + len - 1;
			if (*t == 'F' || *t == 'f' || *t == 'r') {
				*p++ = *t;
				*t-- = '\0';
			}
			switch(*t) {
			case 'b':
				*p++ = 'b';
				*t = '\0';
				break;
			case 'c':
				*p++ = 'c';
				*t = '\0';
				break;
			case 'l':
				*t = '\0';
				/* FALLTHROUGH */
			case '0': case '1': case '2': case '3': case '4':
			case '5': case '6': case '7': case '8': case '9':
				*p++ = 'n';
				break;
			default:
				errx(1, "illegal option -- %s", *argv);
			}
			*p++ = *argv[0];
			(void)strcpy(p, ap);
			*argv = start;
			continue;

		/*
		 * Options w/ arguments, skip the argument and continue
		 * with the next option.
		 */
		case 'b':
		case 'c':
		case 'n':
			if (!ap[1])
				++argv;
			/* FALLTHROUGH */
		/* Options w/o arguments, continue with the next option. */
		case 'F':
		case 'f':
		case 'r':
			continue;

		/* Illegal option, return and let getopt handle it. */
		default:
			return;
		}
	}
}

static void
usage(void)
{
	(void)fprintf(stderr,
	    "usage: tail [-F | -f | -r] [-q] [-b # | -c # | -n #]"
	    " [file ...]\n");
	exit(1);
}
