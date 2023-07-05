/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2000 Sheldon Hearn <sheldonh@FreeBSD.org>.
 * All rights reserved.
 *
 * Copyright (c) 2021 The FreeBSD Foundation
 *
 * Portions of this software were developed by Ka Ho Ng <khng@FreeBSD.org>
 * under sponsorship from the FreeBSD Foundation.
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
 *
 */

static const char rcsid[] =
    "$FreeBSD$";

#include <sys/stat.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <libutil.h>

static void	usage(void);

int
main(int argc, char **argv)
{
	struct stat sb;
	mode_t omode;
	off_t oflow, rsize, sz, tsize, round, off, len;
	uint64_t usz;
	int ch, error, fd, oflags, r;
	int do_dealloc;
	int do_truncate;
	int no_create;
	int do_relative;
	int do_round;
	int do_refer;
	int got_size;
	char *fname, *rname;
	struct spacectl_range sr;

	fd = -1;
	rsize = tsize = sz = off = 0;
	len = -1;
	do_dealloc = no_create = do_relative = do_round = do_refer =
	    got_size = 0;
	do_truncate = 1;
	error = r = 0;
	rname = NULL;
	while ((ch = getopt(argc, argv, "cdr:s:o:l:")) != -1)
		switch (ch) {
		case 'c':
			no_create = 1;
			break;
		case 'd':
			do_dealloc = 1;
			do_truncate = 0;
			break;
		case 'r':
			do_refer = 1;
			rname = optarg;
			break;
		case 's':
			if (*optarg == '+' || *optarg == '-') {
				do_relative = 1;
			} else if (*optarg == '%' || *optarg == '/') {
				do_round = 1;
			}
			if (expand_number(do_relative || do_round ?
			    optarg + 1 : optarg,
			    &usz) == -1 || (off_t)usz < 0)
				errx(EXIT_FAILURE,
				    "invalid size argument `%s'", optarg);

			sz = (*optarg == '-' || *optarg == '/') ?
				-(off_t)usz : (off_t)usz;
			got_size = 1;
			break;
		case 'o':
			if (expand_number(optarg, &usz) == -1 ||
			    (off_t)usz < 0)
				errx(EXIT_FAILURE,
				    "invalid offset argument `%s'", optarg);

			off = usz;
			break;
		case 'l':
			if (expand_number(optarg, &usz) == -1 ||
			    (off_t)usz <= 0)
				errx(EXIT_FAILURE,
				    "invalid length argument `%s'", optarg);

			len = usz;
			break;
		default:
			usage();
			/* NOTREACHED */
		}

	argv += optind;
	argc -= optind;

	/*
	 * Exactly one of do_refer, got_size or do_dealloc must be specified.
	 * Since do_relative implies got_size, do_relative, do_refer and
	 * do_dealloc are also mutually exclusive.  If do_dealloc is specified,
	 * the length argument must be set.  See usage() for allowed
	 * invocations.
	 */
	if (argc < 1 || do_refer + got_size + do_dealloc != 1 ||
	    (do_dealloc == 1 && len == -1))
		usage();
	if (do_refer == 1) {
		if (stat(rname, &sb) == -1)
			err(EXIT_FAILURE, "%s", rname);
		tsize = sb.st_size;
	} else if (do_relative == 1 || do_round == 1)
		rsize = sz;
	else if (do_dealloc == 0)
		tsize = sz;

	if (no_create)
		oflags = O_WRONLY;
	else
		oflags = O_WRONLY | O_CREAT;
	omode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;

	while ((fname = *argv++) != NULL) {
		if (fd != -1)
			close(fd);
		if ((fd = open(fname, oflags, omode)) == -1) {
			if (errno != ENOENT) {
				warn("%s", fname);
				error++;
			}
			continue;
		}
		if (do_relative == 1) {
			if (fstat(fd, &sb) == -1) {
				warn("%s", fname);
				error++;
				continue;
			}
			oflow = sb.st_size + rsize;
			if (oflow < (sb.st_size + rsize)) {
				errno = EFBIG;
				warn("%s", fname);
				error++;
				continue;
			}
			tsize = oflow;
		}
		if (do_round == 1) {
			if (fstat(fd, &sb) == -1) {
				warn("%s", fname);
				error++;
				continue;
			}
			sz = rsize;
			if (sz < 0)
				sz = -sz;
			if (sb.st_size % sz) {
				round = sb.st_size / sz;
				if (round != sz && rsize < 0)
					round--;
				else if (rsize > 0)
					round++;
				tsize = (round < 0 ? 0 : round) * sz;
			} else
				tsize = sb.st_size;
		}
		if (tsize < 0)
			tsize = 0;

		if (do_dealloc == 1) {
			sr.r_offset = off;
			sr.r_len = len;
			r = fspacectl(fd, SPACECTL_DEALLOC, &sr, 0, &sr);
		}
		if (do_truncate == 1)
			r = ftruncate(fd, tsize);
		if (r == -1) {
			warn("%s", fname);
			error++;
		}
	}
	if (fd != -1)
		close(fd);

	return (error ? EXIT_FAILURE : EXIT_SUCCESS);
}

static void
usage(void)
{
	fprintf(stderr, "%s\n%s\n%s\n",
	    "usage: truncate [-c] -s [+|-|%|/]size[K|k|M|m|G|g|T|t] file ...",
	    "       truncate [-c] -r rfile file ...",
	    "       truncate [-c] -d [-o offset[K|k|M|m|G|g|T|t]] -l length[K|k|M|m|G|g|T|t] file ...");
	exit(EXIT_FAILURE);
}
