/*-
 * Copyright (c) 2026 Dag-Erling Smørgrav <des@FreeBSD.org>
 * Copyright (c) 2015-2021 Baptiste Daroussin <bapt@FreeBSD.org>
 * Copyright (c) 2015 Xin LI <delphij@FreeBSD.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/capsicum.h>
#include <sys/sbuf.h>

#include <capsicum_helpers.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <getopt.h>
#include <locale.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <libcasper.h>
#include <casper/cap_fileargs.h>

typedef enum {
	/* state	condition to transit to next state */
	INIT,		/* '$' */
	DELIM_SEEN,	/* letter */
	KEYWORD,	/* punctuation mark */
	PUNC_SEEN,	/* ':' -> _SVN; space -> TEXT */
	PUNC_SEEN_SVN,	/* space */
	TEXT
} analyzer_states;

static int
scan(FILE *fp, const char *name, bool quiet)
{
	int c;
	bool hasid = false;
	bool subversion = false;
	analyzer_states state = INIT;
	FILE *buffp;
	char *buf;
	size_t sz;

	sz = 0;
	buf = NULL;
	if ((buffp = open_memstream(&buf, &sz)) == NULL)
		goto bufferr;

	if (name != NULL) {
		printf("%s:\n", name);
		if (fflush(stdout) == EOF)
			err(EXIT_FAILURE, "stdout");
	}

	while ((c = fgetc(fp)) != EOF) {
		switch (state) {
		case INIT:
			if (c == '$') {
				/* Transit to DELIM_SEEN if we see $ */
				state = DELIM_SEEN;
			} else {
				/* Otherwise, stay in INIT state */
				continue;
			}
			break;
		case DELIM_SEEN:
			if (isalpha(c)) {
				/* Transit to KEYWORD if we see letter */
				if (buf != NULL)
					memset(buf, 0, sz);
				if (fseek(buffp, 0, SEEK_SET) != 0 ||
				    fputc('$', buffp) == EOF ||
				    fputc(c, buffp) == EOF)
					goto bufferr;
				state = KEYWORD;

				continue;
			} else if (c == '$') {
				/* Or, stay in DELIM_SEEN if more $ */
				continue;
			} else {
				/* Otherwise, transit back to INIT */
				state = INIT;
			}
			break;
		case KEYWORD:
			if (fputc(c, buffp) == EOF)
				goto bufferr;

			if (isalpha(c)) {
				/*
				 * Stay in KEYWORD if additional letter is seen
				 */
				continue;
			} else if (c == ':') {
				/*
				 * See ':' for the first time, transit to
				 * PUNC_SEEN.
				 */
				state = PUNC_SEEN;
				subversion = false;
			} else if (c == '$') {
				/*
				 * Incomplete ident.  Go back to DELIM_SEEN
				 * state because we see a '$' which could be
				 * the beginning of a keyword.
				 */
				state = DELIM_SEEN;
			} else {
				/*
				 * Go back to INIT state otherwise.
				 */
				state = INIT;
			}
			break;
		case PUNC_SEEN:
		case PUNC_SEEN_SVN:
			if (fputc(c, buffp) == EOF)
				goto bufferr;

			switch (c) {
			case ':':
				/*
				 * If we see '::' (seen : in PUNC_SEEN),
				 * activate subversion treatment and transit
				 * to PUNC_SEEN_SVN state.
				 *
				 * If more than two :'s were seen, the ident
				 * is invalid and we would therefore go back
				 * to INIT state.
				 */
				if (state == PUNC_SEEN) {
					state = PUNC_SEEN_SVN;
					subversion = true;
				} else {
					state = INIT;
				}
				break;
			case ' ':
				/*
				 * A space after ':' or '::' indicates we are at the
				 * last component of potential ident.
				 */
				state = TEXT;
				break;
			default:
				/* All other characters are invalid */
				state = INIT;
				break;
			}
			break;
		case TEXT:
			if (fputc(c, buffp) == EOF)
				goto bufferr;

			if (iscntrl(c)) {
				/* Control characters are not allowed in this state */
				state = INIT;
			} else if (c == '$') {
				if (fflush(buffp) == EOF)
					goto bufferr;
				/*
				 * valid ident should end with a space.
				 *
				 * subversion extension uses '#' to indicate that
				 * the keyword expansion have exceeded the fixed
				 * width, so it is also permitted if we are in
				 * subversion mode.  No length check is enforced
				 * because GNU RCS ident(1) does not do it either.
				 */
				c = buf[strlen(buf) - 2];
				if (c == ' ' || (subversion && c == '#')) {
					printf("     %s\n", buf);
					if (fflush(stdout) == EOF)
						err(EXIT_FAILURE, "stdout");
					hasid = true;
				}
				state = INIT;
			}
			/* Other characters: stay in the state */
			break;
		}
	}
	if (fclose(buffp) == EOF)
		goto bufferr;
	free(buf);

	if (!hasid) {
		if (!quiet) {
			fprintf(stderr, "%s warning: no id keywords in %s\n",
			    getprogname(), name ? name : "standard input");
		}
		return (EXIT_FAILURE);
	}

	return (EXIT_SUCCESS);
bufferr:
	err(EXIT_FAILURE, "buffer");
}

static void
usage(void)
{
	fprintf(stderr, "usage: %s [-q] [-V] [file...]", getprogname());
	exit(EXIT_FAILURE);
}

static struct option longopts[] = {
	{ "quiet",	no_argument,	NULL,	'q' },
	{ "version",	no_argument,	NULL,	'V' },
	{ NULL,		0,		NULL,	0 }
};

int
main(int argc, char **argv)
{
	fileargs_t *fa;
	cap_rights_t rights;
	bool quiet = false;
	int i, opt, ret;
	FILE *fp;

	setlocale(LC_CTYPE, "C");

	while ((opt = getopt_long(argc, argv, "+qV", longopts, NULL)) != -1) {
		switch (opt) {
		case 'q':
			quiet = true;
			break;
		case 'V':
			/* Do nothing, compat with GNU rcs's ident */
			return (EXIT_SUCCESS);
		default:
			usage();
		}
	}

	argc -= optind;
	argv += optind;

	cap_rights_init(&rights, CAP_READ, CAP_FSTAT, CAP_FCNTL);
	fa = fileargs_init(argc, argv, O_RDONLY, 0, &rights, FA_OPEN);
	if (fa == NULL)
		err(EXIT_FAILURE, "Unable to initialize casper");
	caph_cache_catpages();
	if (caph_limit_stdio() != 0)
		err(EXIT_FAILURE, "Unable to limit stdio");
	if (caph_enter_casper() != 0)
		err(EXIT_FAILURE, "Unable to enter capability mode");

	if (argc == 0) {
		ret = scan(stdin, NULL, quiet);
	} else {
		ret = EXIT_SUCCESS;
		for (i = 0; i < argc; i++) {
			if ((fp = fileargs_fopen(fa, argv[i], "r")) == NULL)
				err(EXIT_FAILURE, "%s", argv[i]);
			if (scan(fp, argv[i], quiet) != EXIT_SUCCESS)
				ret = EXIT_FAILURE;
			(void)fclose(fp);
		}
	}

	fileargs_free(fa);
	return (ret);
}
