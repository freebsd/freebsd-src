/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1980, 1987, 1991, 1993
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
#include <sys/param.h>
#include <sys/stat.h>

#include <capsicum_helpers.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <locale.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wchar.h>
#include <wctype.h>
#include <libxo/xo.h>

#include <libcasper.h>
#include <casper/cap_fileargs.h>

static const char *stdin_filename = "stdin";

static fileargs_t *fa;
static uintmax_t tlinect, twordct, tcharct, tlongline;
static bool doline, doword, dochar, domulti, dolongline;
static volatile sig_atomic_t siginfo;
static xo_handle_t *stderr_handle;

static void	show_cnt(const char *file, uintmax_t linect, uintmax_t wordct,
		    uintmax_t charct, uintmax_t llct);
static int	cnt(const char *);
static void	usage(void);

static void
siginfo_handler(int sig __unused)
{

	siginfo = 1;
}

static void
reset_siginfo(void)
{

	signal(SIGINFO, SIG_DFL);
	siginfo = 0;
}

int
main(int argc, char *argv[])
{
	int ch, errors, total;
	cap_rights_t rights;

	(void) setlocale(LC_CTYPE, "");

	argc = xo_parse_args(argc, argv);
	if (argc < 0)
		exit(EXIT_FAILURE);

	while ((ch = getopt(argc, argv, "clmwL")) != -1)
		switch((char)ch) {
		case 'l':
			doline = true;
			break;
		case 'w':
			doword = true;
			break;
		case 'c':
			dochar = true;
			domulti = false;
			break;
		case 'L':
			dolongline = true;
			break;
		case 'm':
			domulti = true;
			dochar = false;
			break;
		case '?':
		default:
			usage();
		}
	argv += optind;
	argc -= optind;

	fa = fileargs_init(argc, argv, O_RDONLY, 0,
	    cap_rights_init(&rights, CAP_READ, CAP_FSTAT), FA_OPEN);
	if (fa == NULL)
		xo_err(EXIT_FAILURE, "Unable to initialize casper");
	caph_cache_catpages();
	if (caph_limit_stdio() < 0)
		xo_err(EXIT_FAILURE, "Unable to limit stdio");
	if (caph_enter_casper() < 0)
		xo_err(EXIT_FAILURE, "Unable to enter capability mode");

	/* Wc's flags are on by default. */
	if (!(doline || doword || dochar || domulti || dolongline))
		doline = doword = dochar = true;

	stderr_handle = xo_create_to_file(stderr, XO_STYLE_TEXT, 0);
	xo_open_container("wc");
	xo_open_list("file");

	(void)signal(SIGINFO, siginfo_handler);
	errors = 0;
	total = 0;
	if (argc == 0) {
		xo_open_instance("file");
		if (cnt(NULL) != 0)
			++errors;
		xo_close_instance("file");
	} else {
		while (argc--) {
			xo_open_instance("file");
			if (cnt(*argv++) != 0)
				++errors;
			xo_close_instance("file");
			++total;
		}
	}

	xo_close_list("file");

	if (total > 1) {
		xo_open_container("total");
		show_cnt("total", tlinect, twordct, tcharct, tlongline);
		xo_close_container("total");
	}

	fileargs_free(fa);
	xo_close_container("wc");
	if (xo_finish() < 0)
		xo_err(EXIT_FAILURE, "stdout");
	exit(errors == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
}

static void
show_cnt(const char *file, uintmax_t linect, uintmax_t wordct,
    uintmax_t charct, uintmax_t llct)
{
	xo_handle_t *xop;

	if (!siginfo)
		xop = NULL;
	else {
		xop = stderr_handle;
		siginfo = 0;
	}

	if (doline)
		xo_emit_h(xop, " {:lines/%7ju/%ju}", linect);
	if (doword)
		xo_emit_h(xop, " {:words/%7ju/%ju}", wordct);
	if (dochar || domulti)
		xo_emit_h(xop, " {:characters/%7ju/%ju}", charct);
	if (dolongline)
		xo_emit_h(xop, " {:long-lines/%7ju/%ju}", llct);
	if (file != stdin_filename)
		xo_emit_h(xop, " {:filename/%s}\n", file);
	else
		xo_emit_h(xop, "\n");
}

static int
cnt(const char *file)
{
	static char buf[MAXBSIZE];
	struct stat sb;
	mbstate_t mbs;
	const char *p;
	uintmax_t linect, wordct, charct, llct, tmpll;
	ssize_t len;
	size_t clen;
	int fd;
	wchar_t wch;
	bool gotsp, warned;

	linect = wordct = charct = llct = tmpll = 0;
	if (file == NULL) {
		fd = STDIN_FILENO;
		file = stdin_filename;
	} else if ((fd = fileargs_open(fa, file)) < 0) {
		xo_warn("%s: open", file);
		return (1);
	}
	if (doword || (domulti && MB_CUR_MAX != 1))
		goto word;
	/*
	 * If all we need is the number of characters and it's a regular file,
	 * just stat it.
	 */
	if (doline == 0 && dolongline == 0) {
		if (fstat(fd, &sb)) {
			xo_warn("%s: fstat", file);
			(void)close(fd);
			return (1);
		}
		/* pseudo-filesystems advertize a zero size */
		if (S_ISREG(sb.st_mode) && sb.st_size > 0) {
			reset_siginfo();
			charct = sb.st_size;
			show_cnt(file, linect, wordct, charct, llct);
			tcharct += charct;
			(void)close(fd);
			return (0);
		}
	}
	/*
	 * For files we can't stat, or if we need line counting, slurp the
	 * file.  Line counting is split out because it's a lot faster to get
	 * lines than to get words, since the word count requires locale
	 * handling.
	 */
	while ((len = read(fd, buf, sizeof(buf))) != 0) {
		if (len < 0) {
			xo_warn("%s: read", file);
			(void)close(fd);
			return (1);
		}
		if (siginfo)
			show_cnt(file, linect, wordct, charct, llct);
		charct += len;
		if (doline || dolongline) {
			for (p = buf; len > 0; --len, ++p) {
				if (*p == '\n') {
					if (tmpll > llct)
						llct = tmpll;
					tmpll = 0;
					++linect;
				} else {
					tmpll++;
				}
			}
		}
	}
	reset_siginfo();
	if (doline)
		tlinect += linect;
	if (dochar)
		tcharct += charct;
	if (dolongline && llct > tlongline)
		tlongline = llct;
	show_cnt(file, linect, wordct, charct, llct);
	(void)close(fd);
	return (0);

	/* Do it the hard way... */
word:	gotsp = true;
	warned = false;
	memset(&mbs, 0, sizeof(mbs));
	while ((len = read(fd, buf, sizeof(buf))) != 0) {
		if (len < 0) {
			xo_warn("%s: read", file);
			(void)close(fd);
			return (1);
		}
		p = buf;
		while (len > 0) {
			if (siginfo)
				show_cnt(file, linect, wordct, charct, llct);
			if (!domulti || MB_CUR_MAX == 1) {
				clen = 1;
				wch = (unsigned char)*p;
			} else if ((clen = mbrtowc(&wch, p, len, &mbs)) == 0) {
				clen = 1;
			} else if (clen == (size_t)-1) {
				if (!warned) {
					errno = EILSEQ;
					xo_warn("%s", file);
					warned = true;
				}
				memset(&mbs, 0, sizeof(mbs));
				clen = 1;
				wch = (unsigned char)*p;
			} else if (clen == (size_t)-2) {
				break;
			}
			charct++;
			if (wch != L'\n')
				tmpll++;
			len -= clen;
			p += clen;
			if (wch == L'\n') {
				if (tmpll > llct)
					llct = tmpll;
				tmpll = 0;
				++linect;
			}
			if (iswspace(wch)) {
				gotsp = true;
			} else if (gotsp) {
				gotsp = false;
				++wordct;
			}
		}
	}
	reset_siginfo();
	if (domulti && MB_CUR_MAX > 1) {
		if (mbrtowc(NULL, NULL, 0, &mbs) == (size_t)-1 && !warned)
			xo_warn("%s", file);
	}
	if (doline)
		tlinect += linect;
	if (doword)
		twordct += wordct;
	if (dochar || domulti)
		tcharct += charct;
	if (dolongline && llct > tlongline)
		tlongline = llct;
	show_cnt(file, linect, wordct, charct, llct);
	(void)close(fd);
	return (0);
}

static void
usage(void)
{
	xo_error("usage: wc [-Lclmw] [file ...]\n");
	exit(EXIT_FAILURE);
}
