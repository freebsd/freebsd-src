/*-
 * Copyright (c) 2013 Juniper Networks, Inc.
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

#include <sys/queue.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <err.h>
#include <fcntl.h>
#include <libutil.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#include "mkimg.h"
#include "scheme.h"

#define	BUFFER_SIZE	(1024*1024)

struct partlisthead partlist = STAILQ_HEAD_INITIALIZER(partlist);
u_int nparts = 0;

static int bcfd = 0;
static int outfd = 0;
static int tmpfd = -1;

static char tmpfname[] = "/tmp/mkimg-XXXXXX";

static void
cleanup(void)
{

	if (tmpfd != -1)
		close(tmpfd);
	unlink(tmpfname);
}

static void
usage(const char *why)
{
	warnx("error: %s", why);
	fprintf(stderr, "usage: %s <options>\n", getprogname());
	fprintf(stderr, "    options:\n");
	fprintf(stderr, "\t-b <bootcode>\n");
	fprintf(stderr, "\t-o <file>\n");
	fprintf(stderr, "\t-p <partition>\n");
	fprintf(stderr, "\t-s <scheme>\n");
	fprintf(stderr, "\t-z\n");
	exit(EX_USAGE);
}

/*
 * A partition specification has the following format:
 *	<type> ':' <kind> <contents>
 * where:
 *	type	  the partition type alias
 *	kind	  the interpretation of the contents specification
 *		  ':'   contents holds the size of an empty partition
 *		  '='   contents holds the name of a file to read
 *		  '!'   contents holds a command to run; the output of
 *			which is the contents of the partition.
 *	contents  the specification of a partition's contents
 */
static int
parse_part(const char *spec)
{
	struct part *part;
	char *sep;
	size_t len;
	int error;

	part = calloc(1, sizeof(struct part));
	if (part == NULL)
		return (ENOMEM);

	sep = strchr(spec, ':');
	if (sep == NULL) {
		error = EINVAL;
		goto errout;
	}
	len = sep - spec + 1;
	if (len < 2) {
		error = EINVAL;
		goto errout;
	}
	part->type = malloc(len);
	if (part->type == NULL) {
		error = ENOMEM;
		goto errout;
	}
	strlcpy(part->type, spec, len);
	spec = sep + 1;

	switch (*spec) {
	case ':':
		part->kind = PART_KIND_SIZE;
		break;
	case '=':
		part->kind = PART_KIND_FILE;
		break;
	case '!':
		part->kind = PART_KIND_PIPE;
		break;
	default:
		error = EINVAL;
		goto errout;
	}
	spec++;

	part->contents = strdup(spec);
	if (part->contents == NULL) {
		error = ENOMEM;
		goto errout;
	}

	part->index = nparts;
	STAILQ_INSERT_TAIL(&partlist, part, link);
	nparts++;
	return (0);

 errout:
	if (part->type != NULL)
		free(part->type);
	free(part);
	return (error);
}

static int
fdcopy(int src, int dst, uint64_t *count)
{
	void *buffer;
	ssize_t rdsz, wrsz;

	if (count != 0)
		*count = 0;

	buffer = malloc(BUFFER_SIZE);
	if (buffer == NULL)
		return (errno);
	while (1) {
		rdsz = read(src, buffer, BUFFER_SIZE);
		if (rdsz <= 0) {
			free(buffer);
			return ((rdsz < 0) ? errno : 0);
		}
		if (count != NULL)
			*count += rdsz;
		wrsz = write(dst, buffer, rdsz);
		if (wrsz < 0)
			break;
	}
	free(buffer);
	return (errno);
}

static void
mkimg(void)
{
	FILE *fp;
	struct part *part;
	off_t offset;
	uint64_t size;
	int error, fd;

	if (nparts > scheme_max_parts())
		errc(EX_DATAERR, ENOSPC, "only %d partitions are supported",
		    scheme_max_parts());

	offset = scheme_first_offset(nparts);
	STAILQ_FOREACH(part, &partlist, link) {
		part->offset = offset;
		lseek(tmpfd, offset, SEEK_SET);
		/* XXX check error */

		error = 0;
		switch (part->kind) {
		case PART_KIND_SIZE:
			if (expand_number(part->contents, &size) == -1)
				error = errno;
			break;
		case PART_KIND_FILE:
			fd = open(part->contents, O_RDONLY, 0);
			if (fd != -1) {
				error = fdcopy(fd, tmpfd, &size);
				close(fd);
			} else
				error = errno;
			break;
		case PART_KIND_PIPE:
			fp = popen(part->contents, "r");
			if (fp != NULL) {
				error = fdcopy(fileno(fp), tmpfd, &size);
				pclose(fp);
			} else
				error = errno;
			break;
		}
		part->size = size;
		scheme_check_part(part);
		offset = scheme_next_offset(offset, size);
	}

	scheme_write(tmpfd, offset);
}

int
main(int argc, char *argv[])
{
	int c, error;

	while ((c = getopt(argc, argv, "b:h:o:p:s:t:z")) != -1) {
		switch (c) {
		case 'b':	/* BOOT CODE */
			if (bcfd != 0)
				usage("multiple bootcode given");
			bcfd = open(optarg, O_RDONLY, 0);
			if (bcfd == -1)
				err(EX_UNAVAILABLE, "%s", optarg);
			break;
		case 'h':	/* GEOMETRY: HEADS */
			break;
		case 'o':	/* OUTPUT FILE */
			if (outfd != 0)
				usage("multiple output files given");
			outfd = open(optarg, O_WRONLY | O_CREAT | O_TRUNC,
			    S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH);
			if (outfd == -1)
				err(EX_CANTCREAT, "%s", optarg);
			break;
		case 'p':	/* PARTITION */
			error = parse_part(optarg);
			if (error)
				errc(EX_DATAERR, error, "partition");
			break;
		case 's':	/* SCHEME */
			if (scheme_selected() != SCHEME_UNDEF)
				usage("multiple schemes given");
			error = scheme_select(optarg);
			if (error)
				errc(EX_DATAERR, error, "scheme");
			break;
		case 't':	/* GEOMETRY: TRACK SIZE */
			break;
		case 'z':	/* SPARSE OUTPUT */
			break;
		default:
			usage("unknown option");
		}
	}
	if (argc > optind)
		usage("trailing arguments");
	if (scheme_selected() == SCHEME_UNDEF)
		usage("no scheme");
	if (nparts == 0)
		usage("no partitions");

	if (outfd == 0) {
		if (atexit(cleanup) == -1)
			err(EX_OSERR, "cannot register cleanup function");
		outfd = 1;
		tmpfd = mkstemp(tmpfname);
		if (tmpfd == -1)
			err(EX_OSERR, "cannot create temporary file");
	} else
		tmpfd = outfd;

	mkimg();

	if (tmpfd != outfd) {
		if (lseek(tmpfd, 0, SEEK_SET) == 0)
			error = fdcopy(tmpfd, outfd, NULL);
		else
			error = errno;
		/* XXX check error */
	}

	return (0);
}
