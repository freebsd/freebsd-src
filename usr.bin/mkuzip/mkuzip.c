/*
 * Copyright (c) 2004-2016 Maxim Sobolev <sobomax@FreeBSD.org>
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/disk.h>
#include <sys/endian.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <netinet/in.h>
#include <ctype.h>
#include <err.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mkuzip.h"
#include "mkuz_cloop.h"
#include "mkuz_blockcache.h"
#include "mkuz_zlib.h"
#include "mkuz_lzma.h"

#define DEFINE_RAW_METHOD(func, rval, args...) typedef rval (*func##_t)(args)

#define DEFAULT_CLSTSIZE	16384

DEFINE_RAW_METHOD(f_init, void *, uint32_t);
DEFINE_RAW_METHOD(f_compress, void, const char *, uint32_t *);

struct mkuz_format {
	const char *magic;
	const char *default_sufx;
	f_init_t f_init;
	f_compress_t f_compress;
};

static struct mkuz_format uzip_fmt = {
	.magic = CLOOP_MAGIC_ZLIB,
	.default_sufx = DEFAULT_SUFX_ZLIB,
	.f_init = &mkuz_zlib_init,
	.f_compress = &mkuz_zlib_compress
};

static struct mkuz_format ulzma_fmt = {
        .magic = CLOOP_MAGIC_LZMA,
        .default_sufx = DEFAULT_SUFX_LZMA,
        .f_init = &mkuz_lzma_init,
        .f_compress = &mkuz_lzma_compress
};

static char *readblock(int, char *, u_int32_t);
static void usage(void);
static void cleanup(void);
static int  memvcmp(const void *, unsigned char, size_t);

static char *cleanfile = NULL;

int main(int argc, char **argv)
{
	char *iname, *oname, *obuf, *ibuf;
	uint64_t *toc;
	int fdr, fdw, i, opt, verbose, no_zcomp, tmp, en_dedup;
	struct iovec iov[2];
	struct stat sb;
	uint32_t destlen;
	uint64_t offset, last_offset;
	struct cloop_header hdr;
	struct mkuz_blkcache_hit *chit;
	const struct mkuz_format *handler;

	memset(&hdr, 0, sizeof(hdr));
	hdr.blksz = DEFAULT_CLSTSIZE;
	oname = NULL;
	verbose = 0;
	no_zcomp = 0;
	en_dedup = 0;
	handler = &uzip_fmt;

	while((opt = getopt(argc, argv, "o:s:vZdL")) != -1) {
		switch(opt) {
		case 'o':
			oname = optarg;
			break;

		case 's':
			tmp = atoi(optarg);
			if (tmp <= 0) {
				errx(1, "invalid cluster size specified: %s",
				    optarg);
				/* Not reached */
			}
			hdr.blksz = tmp;
			break;

		case 'v':
			verbose = 1;
			break;

		case 'Z':
			no_zcomp = 1;
			break;

		case 'd':
			en_dedup = 1;
			break;

		case 'L':
			handler = &ulzma_fmt;
			break;

		default:
			usage();
			/* Not reached */
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 1) {
		usage();
		/* Not reached */
	}

	strcpy(hdr.magic, handler->magic);

	if (en_dedup != 0) {
		hdr.magic[CLOOP_OFS_VERSN] = CLOOP_MAJVER_3;
		hdr.magic[CLOOP_OFS_COMPR] =
		    tolower(hdr.magic[CLOOP_OFS_COMPR]);
	}

	obuf = handler->f_init(hdr.blksz);

	iname = argv[0];
	if (oname == NULL) {
		asprintf(&oname, "%s%s", iname, handler->default_sufx);
		if (oname == NULL) {
			err(1, "can't allocate memory");
			/* Not reached */
		}
	}

	ibuf = mkuz_safe_malloc(hdr.blksz);

	signal(SIGHUP, exit);
	signal(SIGINT, exit);
	signal(SIGTERM, exit);
	signal(SIGXCPU, exit);
	signal(SIGXFSZ, exit);
	atexit(cleanup);

	fdr = open(iname, O_RDONLY);
	if (fdr < 0) {
		err(1, "open(%s)", iname);
		/* Not reached */
	}
	if (fstat(fdr, &sb) != 0) {
		err(1, "fstat(%s)", iname);
		/* Not reached */
	}
	if (S_ISCHR(sb.st_mode)) {
		off_t ms;

		if (ioctl(fdr, DIOCGMEDIASIZE, &ms) < 0) {
			err(1, "ioctl(DIOCGMEDIASIZE)");
			/* Not reached */
		}
		sb.st_size = ms;
	} else if (!S_ISREG(sb.st_mode)) {
		fprintf(stderr, "%s: not a character device or regular file\n",
			iname);
		exit(1);
	}
	hdr.nblocks = sb.st_size / hdr.blksz;
	if ((sb.st_size % hdr.blksz) != 0) {
		if (verbose != 0)
			fprintf(stderr, "file size is not multiple "
			"of %d, padding data\n", hdr.blksz);
		hdr.nblocks++;
	}
	toc = mkuz_safe_malloc((hdr.nblocks + 1) * sizeof(*toc));

	fdw = open(oname, O_WRONLY | O_TRUNC | O_CREAT,
		   S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
	if (fdw < 0) {
		err(1, "open(%s)", oname);
		/* Not reached */
	}
	cleanfile = oname;

	/* Prepare header that we will write later when we have index ready. */
	iov[0].iov_base = (char *)&hdr;
	iov[0].iov_len = sizeof(hdr);
	iov[1].iov_base = (char *)toc;
	iov[1].iov_len = (hdr.nblocks + 1) * sizeof(*toc);
	offset = iov[0].iov_len + iov[1].iov_len;

	/* Reserve space for header */
	lseek(fdw, offset, SEEK_SET);

	if (verbose != 0)
		fprintf(stderr, "data size %ju bytes, number of clusters "
		    "%u, index length %zu bytes\n", sb.st_size,
		    hdr.nblocks, iov[1].iov_len);

	last_offset = 0;
	for(i = 0; i == 0 || ibuf != NULL; i++) {
		ibuf = readblock(fdr, ibuf, hdr.blksz);
		if (ibuf != NULL) {
			if (no_zcomp == 0 && \
			    memvcmp(ibuf, '\0', hdr.blksz) != 0) {
				/* All zeroes block */
				destlen = 0;
			} else {
				handler->f_compress(ibuf, &destlen);
			}
		} else {
			destlen = DEV_BSIZE - (offset % DEV_BSIZE);
			memset(obuf, 0, destlen);
			if (verbose != 0)
				fprintf(stderr, "padding data with %lu bytes "
				    "so that file size is multiple of %d\n",
				    (u_long)destlen, DEV_BSIZE);
		}
		if (destlen > 0 && en_dedup != 0) {
			chit = mkuz_blkcache_regblock(fdw, i, offset, destlen,
			    obuf);
			/*
			 * There should be at least one non-empty block
			 * between us and the backref'ed offset, otherwise
			 * we won't be able to parse that sequence correctly
			 * as it would be indistinguishible from another
			 * empty block.
			 */
			if (chit != NULL && chit->offset == last_offset) {
				chit = NULL;
			}
		} else {
			chit = NULL;
		}
		if (chit != NULL) {
			toc[i] = htobe64(chit->offset);
		} else {
			if (destlen > 0 && write(fdw, obuf, destlen) < 0) {
				err(1, "write(%s)", oname);
				/* Not reached */
			}
			toc[i] = htobe64(offset);
			last_offset = offset;
			offset += destlen;
		}
		if (ibuf != NULL && verbose != 0) {
			fprintf(stderr, "cluster #%d, in %u bytes, "
			    "out len=%lu offset=%lu", i, hdr.blksz,
			    chit == NULL ? (u_long)destlen : 0,
			    (u_long)be64toh(toc[i]));
			if (chit != NULL) {
				fprintf(stderr, " (backref'ed to #%d)",
				    chit->blkno);
			}
			fprintf(stderr, "\n");

		}
	}
	close(fdr);

	if (verbose != 0)
		fprintf(stderr, "compressed data to %ju bytes, saved %lld "
		    "bytes, %.2f%% decrease.\n", offset,
		    (long long)(sb.st_size - offset),
		    100.0 * (long long)(sb.st_size - offset) /
		    (float)sb.st_size);

	/* Convert to big endian */
	hdr.blksz = htonl(hdr.blksz);
	hdr.nblocks = htonl(hdr.nblocks);
	/* Write headers into pre-allocated space */
	lseek(fdw, 0, SEEK_SET);
	if (writev(fdw, iov, 2) < 0) {
		err(1, "writev(%s)", oname);
		/* Not reached */
	}
	cleanfile = NULL;
	close(fdw);

	exit(0);
}

static char *
readblock(int fd, char *ibuf, u_int32_t clstsize)
{
	int numread;

	bzero(ibuf, clstsize);
	numread = read(fd, ibuf, clstsize);
	if (numread < 0) {
		err(1, "read() failed");
		/* Not reached */
	}
	if (numread == 0) {
		return NULL;
	}
	return ibuf;
}

static void
usage(void)
{

	fprintf(stderr, "usage: mkuzip [-vZdL] [-o outfile] [-s cluster_size] "
	    "infile\n");
	exit(1);
}

void *
mkuz_safe_malloc(size_t size)
{
	void *retval;

	retval = malloc(size);
	if (retval == NULL) {
		err(1, "can't allocate memory");
		/* Not reached */
	}
	return retval;
}

static void
cleanup(void)
{

	if (cleanfile != NULL)
		unlink(cleanfile);
}

static int
memvcmp(const void *memory, unsigned char val, size_t size)
{
    const u_char *mm;

    mm = (const u_char *)memory;
    return (*mm == val) && memcmp(mm, mm + 1, size - 1) == 0;
}
