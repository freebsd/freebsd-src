/*
 * ----------------------------------------------------------------------------
 * Derived from mkuzip.c by Aleksandr Rybalko <ray@ddteam.net>
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <sobomax@FreeBSD.ORG> wrote this file. As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.       Maxim Sobolev
 * ----------------------------------------------------------------------------
 *
 * $FreeBSD$
 *
 */

#include <sys/disk.h>
#include <sys/endian.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <netinet/in.h>
#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <lzma.h>

#define CLSTSIZE	16384
#define DEFAULT_SUFX	".ulzma"

#define USED_BLOCKSIZE DEV_BSIZE

#define CLOOP_MAGIC_LEN 128
/* Format L3.0, since we move to XZ API */
static char CLOOP_MAGIC_START[] =
    "#!/bin/sh\n"
    "#L3.0\n"
    "n=uncompress\n"
    "m=geom_$n\n"
    "(kldstat -m $m 2>&-||kldload $m)>&-&&"
	"mount_cd9660 /dev/`mdconfig -af $0`.$n $1\n"
    "exit $?\n";

static char *readblock(int, char *, u_int32_t);
static void usage(void);
static void *safe_malloc(size_t);
static void cleanup(void);

static char *cleanfile = NULL;

int main(int argc, char **argv)
{
	char *iname, *oname, *obuf, *ibuf;
	int fdr, fdw, i, opt, verbose, tmp;
	struct iovec iov[2];
	struct stat sb;
	uint32_t destlen;
	uint64_t offset;
	uint64_t *toc;
	lzma_filter filters[2];
	lzma_options_lzma opt_lzma;
	lzma_ret ret;
	lzma_stream strm = LZMA_STREAM_INIT;
	struct cloop_header {
		char magic[CLOOP_MAGIC_LEN];    /* cloop magic */
		uint32_t blksz;                 /* block size */
		uint32_t nblocks;               /* number of blocks */
	} hdr;

	memset(&hdr, 0, sizeof(hdr));
	hdr.blksz = CLSTSIZE;
	strcpy(hdr.magic, CLOOP_MAGIC_START);
	oname = NULL;
	verbose = 0;

	while((opt = getopt(argc, argv, "o:s:v")) != -1) {
		switch(opt) {
		case 'o':
			oname = optarg;
			break;

		case 's':
			tmp = atoi(optarg);
			if (tmp <= 0) {
				errx(1,
				    "invalid cluster size specified: %s",
				    optarg);
				/* Not reached */
			}
			if (tmp % USED_BLOCKSIZE != 0) {
				errx(1,
				    "cluster size should be multiple of %d",
				    USED_BLOCKSIZE);
				/* Not reached */
			}
			if ( tmp > MAXPHYS) {
				errx(1, "cluster size is too large");
				    /* Not reached */
			}
			hdr.blksz = tmp;
			break;

		case 'v':
			verbose = 1;
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

	iname = argv[0];
	if (oname == NULL) {
		asprintf(&oname, "%s%s", iname, DEFAULT_SUFX);
		if (oname == NULL) {
			err(1, "can't allocate memory");
			/* Not reached */
		}
	}

	obuf = safe_malloc(hdr.blksz*2);
	ibuf = safe_malloc(hdr.blksz);

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
		fprintf(stderr,
		    "%s: not a character device or regular file\n",
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
	toc = safe_malloc((hdr.nblocks + 1) * sizeof(*toc));

	fdw = open(oname, O_WRONLY | O_TRUNC | O_CREAT,
		   S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
	if (fdw < 0) {
		err(1, "open(%s)", oname);
		/* Not reached */
	}
	cleanfile = oname;

	/*
	 * Prepare header that we will write later when we have index ready.
	 */
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

	/* Init lzma encoder */
	if (lzma_lzma_preset(&opt_lzma, LZMA_PRESET_DEFAULT))
		errx(1, "Error loading LZMA preset");

	filters[0].id = LZMA_FILTER_LZMA2;
	filters[0].options = &opt_lzma;
	filters[1].id = LZMA_VLI_UNKNOWN;

	for(i = 0; i == 0 || ibuf != NULL; i++) {
		ibuf = readblock(fdr, ibuf, hdr.blksz);
		if (ibuf != NULL) {
			destlen = hdr.blksz*2;

			ret = lzma_stream_encoder(&strm, filters,
			    LZMA_CHECK_CRC32);
			if (ret != LZMA_OK) {
				if (ret == LZMA_MEMLIMIT_ERROR)
					errx(1, "can't compress data: "
					    "LZMA_MEMLIMIT_ERROR");

				errx(1, "can't compress data: "
				    "LZMA compressor ERROR");
			}

			strm.next_in = ibuf;
			strm.avail_in = hdr.blksz;
			strm.next_out = obuf;
			strm.avail_out = hdr.blksz*2;

			ret = lzma_code(&strm, LZMA_FINISH);

			if (ret != LZMA_STREAM_END) {
				/* Error */
				errx(1, "lzma_code FINISH failed, code=%d, "
				    "pos(in=%zd, out=%zd)",
				    ret,
				    (hdr.blksz - strm.avail_in),
				    (hdr.blksz*2 - strm.avail_out));
			}

			destlen -= strm.avail_out;

			lzma_end(&strm);

			if (verbose != 0)
				fprintf(stderr, "cluster #%d, in %u bytes, "
				    "out %u bytes\n", i, hdr.blksz, destlen);
		} else {
			destlen = USED_BLOCKSIZE - (offset % USED_BLOCKSIZE);
			memset(obuf, 0, destlen);
			if (verbose != 0)
				fprintf(stderr, "padding data with %u bytes"
				    " so that file size is multiple of %d\n",
				    destlen,
				    USED_BLOCKSIZE);
		}
		if (write(fdw, obuf, destlen) < 0) {
			err(1, "write(%s)", oname);
			/* Not reached */
		}
		toc[i] = htobe64(offset);
		offset += destlen;
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

	fprintf(stderr, "usage: mkulzma [-v] [-o outfile] [-s cluster_size] "
	    "infile\n");
	exit(1);
}

static void *
safe_malloc(size_t size)
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
