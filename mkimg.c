/*-
 * Copyright (c) 2013,2014 Juniper Networks, Inc.
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

#include <sys/linker_set.h>
#include <sys/queue.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <err.h>
#include <fcntl.h>
#include <libutil.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#include "mkimg.h"
#include "scheme.h"

#if !defined(SPARSE_WRITE)
#define	sparse_write	write
#endif

#define	BUFFER_SIZE	(1024*1024)

struct partlisthead partlist = STAILQ_HEAD_INITIALIZER(partlist);
u_int nparts = 0;

u_int verbose;

u_int ncyls = 0;
u_int nheads = 1;
u_int nsecs = 1;
u_int secsz = 512;
u_int blksz = 0;

static int bcfd = -1;
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
	struct mkimg_scheme *s, **iter;

	warnx("error: %s", why);
	fprintf(stderr, "\nusage: %s <options>\n", getprogname());

	fprintf(stderr, "    options:\n");
	fprintf(stderr, "\t-b <file>\t-  file containing boot code\n");
	fprintf(stderr, "\t-o <file>\t-  file to write image into\n");
	fprintf(stderr, "\t-p <partition>\n");
	fprintf(stderr, "\t-s <scheme>\n");
	fprintf(stderr, "\t-H <num>\t-  number of heads to simulate\n");
	fprintf(stderr, "\t-P <num>\t-  physical sector size\n");
	fprintf(stderr, "\t-S <num>\t-  logical sector size\n");
	fprintf(stderr, "\t-T <num>\t-  number of tracks to simulate\n");

	fprintf(stderr, "    schemes:\n");
	SET_FOREACH(iter, schemes) {
		s = *iter;
		fprintf(stderr, "\t%s\t-  %s\n", s->name, s->description);
	}

	fprintf(stderr, "    partition specification:\n");
	fprintf(stderr, "\t<t>[/<l>]::<size>\t-  empty partition of given "
	    "size\n");
	fprintf(stderr, "\t<t>[/<l>]:=<file>\t-  partition content and size "
	    "are determined\n\t\t\t\t   by the named file\n");
	fprintf(stderr, "\t<t>[/<l>]:-<cmd>\t-  partition content and size "
	    "are taken from\n\t\t\t\t   the output of the command to run\n");
	fprintf(stderr, "\t    where:\n");
	fprintf(stderr, "\t\t<t>\t-  scheme neutral partition type\n");
	fprintf(stderr, "\t\t<l>\t-  optional scheme-dependent partition "
	    "label\n");

	exit(EX_USAGE);
}

static int
parse_number(u_int *valp, u_int min, u_int max, const char *arg)
{
	uint64_t val;

	if (expand_number(arg, &val) == -1)
		return (errno);
	if (val > UINT_MAX || val < (uint64_t)min || val > (uint64_t)max)
		return (EINVAL);
	*valp = (u_int)val;
	return (0);
}

static int
pwr_of_two(u_int nr)
{

	return (((nr & (nr - 1)) == 0) ? 1 : 0);
}

/*
 * A partition specification has the following format:
 *	<type> ':' <kind> <contents>
 * where:
 *	type	  the partition type alias
 *	kind	  the interpretation of the contents specification
 *		  ':'   contents holds the size of an empty partition
 *		  '='   contents holds the name of a file to read
 *		  '-'   contents holds a command to run; the output of
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
	part->alias = malloc(len);
	if (part->alias == NULL) {
		error = ENOMEM;
		goto errout;
	}
	strlcpy(part->alias, spec, len);
	spec = sep + 1;

	switch (*spec) {
	case ':':
		part->kind = PART_KIND_SIZE;
		break;
	case '=':
		part->kind = PART_KIND_FILE;
		break;
	case '-':
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

	spec = part->alias;
	sep = strchr(spec, '/');
	if (sep != NULL) {
		*sep++ = '\0';
		if (strlen(part->alias) == 0 || strlen(sep) == 0) {
			error = EINVAL;
			goto errout;
		}
		part->label = strdup(sep);
		if (part->label == NULL) {
			error = ENOMEM;
			goto errout;
		}
	}

	part->index = nparts;
	STAILQ_INSERT_TAIL(&partlist, part, link);
	nparts++;
	return (0);

 errout:
	if (part->alias != NULL)
		free(part->alias);
	free(part);
	return (error);
}

#if defined(SPARSE_WRITE)
static ssize_t
sparse_write(int fd, const char *buf, size_t sz)
{
	const char *p;
	off_t ofs;
	size_t len;
	ssize_t wr, wrsz;

	wrsz = 0;
	p = memchr(buf, 0, sz);
	while (sz > 0) {
		len = (p != NULL) ? (size_t)(p - buf) : sz;
		if (len > 0) {
			len = (len + secsz - 1) & ~(secsz - 1);
			if (len > sz)
				len = sz;
			wr = write(fd, buf, len);
			if (wr < 0)
				return (-1);
		} else {
			while (len < sz && *p++ == '\0')
				len++;
			if (len < sz)
				len &= ~(secsz - 1);
			if (len == 0)
				continue;
			ofs = lseek(fd, len, SEEK_CUR);
			if (ofs < 0)
				return (-1);
			wr = len;
		}
		buf += wr;
		sz -= wr;
		wrsz += wr;
		p = memchr(buf, 0, sz);
	}
	return (wrsz);
}
#endif /* SPARSE_WRITE */

static int
fdcopy(int src, int dst, uint64_t *count)
{
	char *buffer;
	off_t ofs;
	ssize_t rdsz, wrsz;

	/* A return value of -1 means that we can't write a sparse file. */
	ofs = lseek(dst, 0L, SEEK_CUR);

	if (count != NULL)
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
		wrsz = (ofs == -1) ?
		    write(dst, buffer, rdsz) :
		    sparse_write(dst, buffer, rdsz);
		if (wrsz < 0)
			break;
	}
	free(buffer);
	return (errno);
}

int
mkimg_seek(int fd, lba_t blk)
{
	off_t off;

	off = blk * secsz;
	if (lseek(fd, off, SEEK_SET) != off)
		return (errno);
	return (0);
}

static void
mkimg(int bfd)
{
	FILE *fp;
	struct part *part;
	lba_t block;
	off_t bytesize;
	int error, fd;

	error = scheme_bootcode(bfd);
	if (error)
		errc(EX_DATAERR, error, "boot code");

	/* First check partition information */
	STAILQ_FOREACH(part, &partlist, link) {
		error = scheme_check_part(part);
		if (error)
			errc(EX_DATAERR, error, "partition %d", part->index+1);
	}

	block = scheme_metadata(SCHEME_META_IMG_START, 0);
	STAILQ_FOREACH(part, &partlist, link) {
		block = scheme_metadata(SCHEME_META_PART_BEFORE, block);
		if (verbose)
			fprintf(stderr, "partition %d: starting block %llu "
			    "... ", part->index + 1, (long long)block);
		part->block = block;
		error = mkimg_seek(tmpfd, block);
		switch (part->kind) {
		case PART_KIND_SIZE:
			if (expand_number(part->contents, &bytesize) == -1)
				error = errno;
			break;
		case PART_KIND_FILE:
			fd = open(part->contents, O_RDONLY, 0);
			if (fd != -1) {
				error = fdcopy(fd, tmpfd, &bytesize);
				close(fd);
			} else
				error = errno;
			break;
		case PART_KIND_PIPE:
			fp = popen(part->contents, "r");
			if (fp != NULL) {
				error = fdcopy(fileno(fp), tmpfd, &bytesize);
				pclose(fp);
			} else
				error = errno;
			break;
		}
		if (error)
			errc(EX_IOERR, error, "partition %d", part->index + 1);
		part->size = (bytesize + secsz - 1) / secsz;
		if (verbose) {
			bytesize = part->size * secsz;
			fprintf(stderr, "size %llu bytes (%llu blocks)\n",
			     (long long)bytesize, (long long)part->size);
		}
		block = scheme_metadata(SCHEME_META_PART_AFTER,
		    part->block + part->size);
	}

	block = scheme_metadata(SCHEME_META_IMG_END, block);
	error = (scheme_write(tmpfd, block));
}

int
main(int argc, char *argv[])
{
	int c, error;

	while ((c = getopt(argc, argv, "b:o:p:s:vH:P:S:T:")) != -1) {
		switch (c) {
		case 'b':	/* BOOT CODE */
			if (bcfd != -1)
				usage("multiple bootcode given");
			bcfd = open(optarg, O_RDONLY, 0);
			if (bcfd == -1)
				err(EX_UNAVAILABLE, "%s", optarg);
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
			if (scheme_selected() != NULL)
				usage("multiple schemes given");
			error = scheme_select(optarg);
			if (error)
				errc(EX_DATAERR, error, "scheme");
			break;
		case 'v':
			verbose++;
			break;
		case 'H':	/* GEOMETRY: HEADS */
			error = parse_number(&nheads, 1, 255, optarg);
			if (error)
				errc(EX_DATAERR, error, "number of heads");
			break;
		case 'P':	/* GEOMETRY: PHYSICAL SECTOR SIZE */
			error = parse_number(&blksz, 512, INT_MAX+1U, optarg);
			if (error == 0 && !pwr_of_two(blksz))
				error = EINVAL;
			if (error)
				errc(EX_DATAERR, error, "physical sector size");
			break;
		case 'S':	/* GEOMETRY: LOGICAL SECTOR SIZE */
			error = parse_number(&secsz, 512, INT_MAX+1U, optarg);
			if (error == 0 && !pwr_of_two(secsz))
				error = EINVAL;
			if (error)
				errc(EX_DATAERR, error, "logical sector size");
			break;
		case 'T':	/* GEOMETRY: TRACK SIZE */
			error = parse_number(&nsecs, 1, 63, optarg);
			if (error)
				errc(EX_DATAERR, error, "track size");
			break;
		default:
			usage("unknown option");
		}
	}

	if (argc > optind)
		usage("trailing arguments");
	if (scheme_selected() == NULL)
		usage("no scheme");
	if (nparts == 0)
		usage("no partitions");

	if (secsz > blksz) {
		if (blksz != 0)
			errx(EX_DATAERR, "the physical block size cannot "
			    "be smaller than the sector size");
		blksz = secsz;
	}

	if (secsz > scheme_max_secsz())
		errx(EX_DATAERR, "maximum sector size supported is %u; "
		    "size specified is %u", scheme_max_secsz(), secsz);

	if (nparts > scheme_max_parts())
		errx(EX_DATAERR, "%d partitions supported; %d given",
		    scheme_max_parts(), nparts);

	if (outfd == 0) {
		if (atexit(cleanup) == -1)
			err(EX_OSERR, "cannot register cleanup function");
		outfd = 1;
		tmpfd = mkstemp(tmpfname);
		if (tmpfd == -1)
			err(EX_OSERR, "cannot create temporary file");
	} else
		tmpfd = outfd;

	if (verbose) {
		fprintf(stderr, "Logical sector size: %u\n", secsz);
		fprintf(stderr, "Physical block size: %u\n", blksz);
		fprintf(stderr, "Sectors per track:   %u\n", nsecs);
		fprintf(stderr, "Number of heads:     %u\n", nheads);
	}

	mkimg(bcfd);

	if (verbose)
		fprintf(stderr, "Number of cylinders: %u\n", ncyls);

	if (tmpfd != outfd) {
		error = mkimg_seek(tmpfd, 0);
		if (error == 0)
			error = fdcopy(tmpfd, outfd, NULL);
		if (error)
			errc(EX_IOERR, error, "writing to stdout");
	}

	return (0);
}
