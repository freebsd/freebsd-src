/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2018 Eugene Grosbein <eugen@FreeBSD.org>.
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
 *
 */

#include <sys/disk.h>
#include <sys/ioctl.h>
#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <libutil.h>
#include <limits.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <sysexits.h>
#include <unistd.h>

#ifndef lint
static const char rcsid[] =
    "$FreeBSD$";
#endif /* not lint */

static int	trim(char *path, off_t offset, off_t length, int dryrun, int verbose);
static off_t	getsize(char *path);
static void	usage(char *name) __dead2;

int
main(int argc, char **argv)
{
	off_t offset, length;
	uint64_t usz;
	int ch, dryrun, error, verbose;
	char *fname, *name;

	error = 0;
	length = offset = 0;
	name = argv[0];
	dryrun = verbose = 1;

	while ((ch = getopt(argc, argv, "Nfl:o:qr:v")) != -1)
		switch (ch) {
		case 'N':
			dryrun = 1;
			verbose = 1;
			break;
		case 'f':
			dryrun = 0;
			break;
		case 'l':
		case 'o':
			if (expand_number(optarg, &usz) == -1 ||
					(off_t)usz < 0 ||
					(usz == 0 && ch == 'l'))
				errx(EX_USAGE,
					"invalid %s of the region: `%s'",
					ch == 'o' ? "offset" : "length",
					optarg);
			if (ch == 'o')
				offset = (off_t)usz;
			else
				length = (off_t)usz;
			break;
		case 'q':
			verbose = 0;
			break;
		case 'r':
			if ((length = getsize(optarg)) == 0)
				errx(EX_USAGE,
					"invalid zero length reference file"
					" for the region: `%s'", optarg);
			break;
		case 'v':
			verbose = 1;
			break;
		default:
			usage(name);
			/* NOTREACHED */
		}

	argv += optind;
	argc -= optind;

	if (argc < 1)
		usage(name);

	while ((fname = *argv++) != NULL)
		if (trim(fname, offset, length, dryrun, verbose) < 0)
			error++;

	return (error ? EXIT_FAILURE : EXIT_SUCCESS);
}

static off_t
getsize(char *path)
{
	struct stat sb;
	char *tstr;
	off_t mediasize;
	int fd;

	if ((fd = open(path, O_RDONLY | O_DIRECT)) < 0) {
		if (errno == ENOENT && path[0] != '/') {
			if (asprintf(&tstr, "%s%s", _PATH_DEV, path) < 0)
				errx(EX_OSERR, "no memory");
			fd = open(tstr, O_RDONLY | O_DIRECT);
			free(tstr);
		}
	}

	if (fd < 0)
		err(EX_NOINPUT, "`%s'", path);

	if (fstat(fd, &sb) < 0)
		err(EX_IOERR, "`%s'", path);

	if (S_ISREG(sb.st_mode) || S_ISDIR(sb.st_mode)) {
		close(fd);
		return (sb.st_size);
	}

	if (!S_ISCHR(sb.st_mode) && !S_ISBLK(sb.st_mode))
		errx(EX_DATAERR,
			"invalid type of the file "
			"(not regular, directory nor special device): `%s'",
			path);

	if (ioctl(fd, DIOCGMEDIASIZE, &mediasize) < 0)
		errx(EX_UNAVAILABLE,
			"ioctl(DIOCGMEDIASIZE) failed, probably not a disk: "
			"`%s'", path);
	close(fd);

	return (mediasize);
}

static int
trim(char *path, off_t offset, off_t length, int dryrun, int verbose)
{
	off_t arg[2];
	char *tstr;
	int error, fd;

	if (length == 0)
		length = getsize(path);

	if (verbose)
		printf("trim `%s' offset %ju length %ju\n",
			path, (uintmax_t)offset, (uintmax_t)length);

	if (dryrun) {
		printf("dry run: add -f to actually perform the operation\n");
		return (0);
	}

	if ((fd = open(path, O_WRONLY | O_DIRECT)) < 0) {
		if (errno == ENOENT && path[0] != '/') {
			if (asprintf(&tstr, "%s%s", _PATH_DEV, path) < 0)
				errx(EX_OSERR, "no memory");
			fd = open(tstr, O_WRONLY | O_DIRECT);
			free(tstr);
		}
	}
	
	if (fd < 0)
		err(EX_NOINPUT, "`%s'", path);

	arg[0] = offset;
	arg[1] = length;

	error = ioctl(fd, DIOCGDELETE, arg);
	if (error < 0)
		warn("ioctl(DIOCGDELETE) failed for `%s'", path);

	close(fd);
	return (error);
}

static void
usage(char *name)
{
	(void)fprintf(stderr,
	    "usage: %s [-[lo] offset[K|k|M|m|G|g|T|t]] [-r rfile] [-Nfqv] device ...\n",
	    name);
	exit(EX_USAGE);
}
