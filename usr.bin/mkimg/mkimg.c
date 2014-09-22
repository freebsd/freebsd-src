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
#include <sys/uuid.h>
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

#include "image.h"
#include "format.h"
#include "mkimg.h"
#include "scheme.h"

struct partlisthead partlist = STAILQ_HEAD_INITIALIZER(partlist);
u_int nparts = 0;

u_int unit_testing;
u_int verbose;

u_int ncyls = 0;
u_int nheads = 1;
u_int nsecs = 1;
u_int secsz = 512;
u_int blksz = 0;

static void
usage(const char *why)
{
	struct mkimg_format *f, **f_iter;
	struct mkimg_scheme *s, **s_iter;

	warnx("error: %s", why);
	fprintf(stderr, "\nusage: %s <options>\n", getprogname());

	fprintf(stderr, "    options:\n");
	fprintf(stderr, "\t-b <file>\t-  file containing boot code\n");
	fprintf(stderr, "\t-f <format>\n");
	fprintf(stderr, "\t-o <file>\t-  file to write image into\n");
	fprintf(stderr, "\t-p <partition>\n");
	fprintf(stderr, "\t-s <scheme>\n");
	fprintf(stderr, "\t-v\t\t-  increase verbosity\n");
	fprintf(stderr, "\t-y\t\t-  [developers] enable unit test\n");
	fprintf(stderr, "\t-H <num>\t-  number of heads to simulate\n");
	fprintf(stderr, "\t-P <num>\t-  physical sector size\n");
	fprintf(stderr, "\t-S <num>\t-  logical sector size\n");
	fprintf(stderr, "\t-T <num>\t-  number of tracks to simulate\n");

	fprintf(stderr, "\n    formats:\n");
	SET_FOREACH(f_iter, formats) {
		f = *f_iter;
		fprintf(stderr, "\t%s\t-  %s\n", f->name, f->description);
	}

	fprintf(stderr, "\n    schemes:\n");
	SET_FOREACH(s_iter, schemes) {
		s = *s_iter;
		fprintf(stderr, "\t%s\t-  %s\n", s->name, s->description);
	}

	fprintf(stderr, "\n    partition specification:\n");
	fprintf(stderr, "\t<t>[/<l>]::<size>\t-  empty partition of given "
	    "size\n");
	fprintf(stderr, "\t<t>[/<l>]:=<file>\t-  partition content and size "
	    "are determined\n\t\t\t\t   by the named file\n");
	fprintf(stderr, "\t<t>[/<l>]:-<cmd>\t-  partition content and size "
	    "are taken from\n\t\t\t\t   the output of the command to run\n");
	fprintf(stderr, "\t-\t\t\t-  unused partition entry\n");
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
 *
 * A specification that is a single dash indicates an unused partition
 * entry.
 */
static int
parse_part(const char *spec)
{
	struct part *part;
	char *sep;
	size_t len;
	int error;

	if (strcmp(spec, "-") == 0) {
		nparts++;
		return (0);
	}

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
ssize_t
sparse_write(int fd, const void *ptr, size_t sz)
{
	const char *buf, *p;
	off_t ofs;
	size_t len;
	ssize_t wr, wrsz;

	buf = ptr;
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

void
mkimg_uuid(struct uuid *uuid)
{
	static uint8_t gen[sizeof(struct uuid)];
	u_int i;

	if (!unit_testing) {
		uuidgen(uuid, 1);
		return;
	}

	for (i = 0; i < sizeof(gen); i++)
		gen[i]++;
	memcpy(uuid, gen, sizeof(uuid_t));
}

static void
mkimg(void)
{
	FILE *fp;
	struct part *part;
	lba_t block;
	off_t bytesize;
	int error, fd;

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
		switch (part->kind) {
		case PART_KIND_SIZE:
			if (expand_number(part->contents, &bytesize) == -1)
				error = errno;
			break;
		case PART_KIND_FILE:
			fd = open(part->contents, O_RDONLY, 0);
			if (fd != -1) {
				error = image_copyin(block, fd, &bytesize);
				close(fd);
			} else
				error = errno;
			break;
		case PART_KIND_PIPE:
			fp = popen(part->contents, "r");
			if (fp != NULL) {
				fd = fileno(fp);
				error = image_copyin(block, fd, &bytesize);
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
	error = image_set_size(block);
	if (!error)
		error = format_resize(block);
	if (error)
		errc(EX_IOERR, error, "image sizing");
	block = image_get_size();
	ncyls = block / (nsecs * nheads);
	error = (scheme_write(block));
	if (error)
		errc(EX_IOERR, error, "writing metadata");
}

int
main(int argc, char *argv[])
{
	int bcfd, outfd;
	int c, error;

	bcfd = -1;
	outfd = 1;	/* Write to stdout by default */
	while ((c = getopt(argc, argv, "b:f:o:p:s:vyH:P:S:T:")) != -1) {
		switch (c) {
		case 'b':	/* BOOT CODE */
			if (bcfd != -1)
				usage("multiple bootcode given");
			bcfd = open(optarg, O_RDONLY, 0);
			if (bcfd == -1)
				err(EX_UNAVAILABLE, "%s", optarg);
			break;
		case 'f':	/* OUTPUT FORMAT */
			if (format_selected() != NULL)
				usage("multiple formats given");
			error = format_select(optarg);
			if (error)
				errc(EX_DATAERR, error, "format");
			break;
		case 'o':	/* OUTPUT FILE */
			if (outfd != 1)
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
		case 'y':
			unit_testing++;
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

	if (format_selected() == NULL)
		format_select("raw");

	if (bcfd != -1) {
		error = scheme_bootcode(bcfd);
		close(bcfd);
		if (error)
			errc(EX_DATAERR, error, "boot code");
	}

	if (verbose) {
		fprintf(stderr, "Logical sector size: %u\n", secsz);
		fprintf(stderr, "Physical block size: %u\n", blksz);
		fprintf(stderr, "Sectors per track:   %u\n", nsecs);
		fprintf(stderr, "Number of heads:     %u\n", nheads);
		fputc('\n', stderr);
		fprintf(stderr, "Partitioning scheme: %s\n",
		    scheme_selected()->name);
		fprintf(stderr, "Output file format:  %s\n",
		    format_selected()->name);
		fputc('\n', stderr);
	}

	error = image_init();
	if (error)
		errc(EX_OSERR, error, "cannot initialize");

	mkimg();

	if (verbose) {
		fputc('\n', stderr);
		fprintf(stderr, "Number of cylinders: %u\n", ncyls);
	}

	error = format_write(outfd);
	if (error)
		errc(EX_IOERR, error, "writing image");

	return (0);
}
