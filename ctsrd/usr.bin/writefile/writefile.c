/*-
 * Copyright (c) 2012 SRI International
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
 * ("CTSRD"), as part of the DARPA CRASH research programme.
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

#include <sys/param.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <libutil.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#if 0
#include <sys/limits.h>

#endif

#include "eav.h"

static void usage(int) __dead2;

static void
usage(int error)
{
	fprintf(stderr,
"writefile [-hqTvz] [-b <blocksize>] [-i <iseek>] [-l length] [-m md5]\n"
"          [-o <oseek>] <in file> <out file>\n");
	exit(error);
}

static ssize_t
writeall(int fd, const char *buf, size_t len)
{
	size_t wlen = 0;
	ssize_t n;

	while (wlen != len) {
		/* XXX: a progress meter would require smaller writes */
		n = write(fd, buf + wlen, len - wlen);
		if (n < 0) {
			/* XXX: would be polite to use select/poll here */
			if (errno != EAGAIN && errno != EINTR)
				return (n);
		} else
			wlen += n;
	}

	return(len);
}

static off_t
parse_offset(const char *offstr)
{
	char *ep;
	off_t off;

	if (strlen(offstr) > 2 && offstr[0] == '0' && offstr[1] == 'x') {
		errno = 0;
		off = strtoll(offstr, &ep, 16);
		if (*ep != '\0' || (off == 0 && errno != 0))
			return -1;
	} else
		if (expand_number(offstr, &off) != 0)
			return -1;
	return off;
}


int
main(int argc, char *argv[])
{
	char ch, *cp, *digest = NULL;
	unsigned char *ibuf, *obuf = NULL, *vbuf;
	size_t blocksize = 0;
	off_t iseek = 0, oseek = 0, olen = -1, wlen = -1;
	int decompress = 0, ifd, notruncate = 0, ofd, quiet = 0, ret,
	    verify_write = 0;
	enum eav_compression ctype;
	enum eav_digest dtype = EAV_DIGEST_NONE;
	struct stat isb, osb;

	while ((ch = getopt(argc, argv, "b:hi:l:m:o:qTvz")) != -1) {
		switch (ch) {
		case 'b':
			if ((blocksize = parse_offset(optarg)) < 1)
				warnx("Invalid blocksize %s", optarg);
			break;
		case 'h':
			usage(0);
		case 'i':
			if ((iseek = parse_offset(optarg)) < 1)
				warnx("Invalid input seek %s", optarg);
			break;
		case 'l':
			if ((wlen = parse_offset(optarg)) < 1)
				warnx("Invalid length %s", optarg);
			break;
		case 'm':
			if (dtype != EAV_DIGEST_NONE) {
				warnx("Too many digest options");
				usage(1j);
			}
			dtype = EAV_DIGEST_MD5;
			digest = optarg;
			if (strlen(digest) != 32)
				warnx("invalid md5 checksum");
			for (cp = digest; *cp != '\0'; cp++) {
				if (!isxdigit(*cp))
					warnx("invalid md5 checksum");
				*cp = tolower(*cp);
			}
			break;
		case 'o':
			if ((oseek = parse_offset(optarg)) < 1)
				warnx("Invalid output seek %s", optarg);
			break;
		case 'q':
			quiet = 1;
			break;
		case 'T':
			notruncate = 1;
			break;
		case 'v':
			verify_write = 1;
			break;
		case 'z':
			decompress = 1;
			break;
		default:
			usage(1);
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 2)
		usage(1);

	if ((ifd = open(argv[0], O_RDONLY)) == -1)
		err(1, "open(%s)", argv[0]);
	if (fstat(ifd, &isb) == -1)
		err(1, "fstat(%s)", argv[0]);
	if ((ibuf = mmap(NULL, isb.st_size, PROT_READ, 0, ifd, 0)) ==
	    MAP_FAILED)
		err(1, "mmap(%s)", argv[0]);

	if (blocksize == 0) {
		if (stat(argv[1], &osb) == 0 &&
		    (S_ISBLK(osb.st_mode) || S_ISCHR(osb.st_mode)))
			blocksize = osb.st_blksize;
		else
			blocksize = 1;
	}

	if (decompress)
		ctype = eav_taste(ibuf, isb.st_size);
	else
		ctype = EAV_COMP_NONE;

	if ((ret = extract_and_verify(ibuf, isb.st_size, &obuf, &olen, 
	    blocksize, ctype, dtype, digest)) != EAV_SUCCESS)
		errx(1, "failed to extract and verify %s: %s", argv[0],
		    eav_strerror(ret));

	if (!quiet) {
		if (dtype)
			printf("verified %jd input bytes\n", isb.st_size);
		if (ctype && ctype != EAV_COMP_UNKNOWN)
			printf("extracted %jd bytes\n", olen);
	}

	if (wlen == -1)
		wlen = olen - iseek;
	else if (wlen > olen - iseek) {
		if ((obuf = reallocf(obuf, wlen)) == NULL)
			err(1, "realloc");
		memset(obuf + olen, '\0', wlen - (olen - iseek));
	}
	
	if ((ofd = open(argv[1], O_WRONLY|O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO))
	    == -1)
		err(1, "open(%s)", argv[1]);
	if (fstat(ofd, &osb) == -1)
		err(1, "fstat(%s)", argv[1]);
	if (!(osb.st_mode & (S_IFIFO|S_IFCHR|S_IFBLK|S_IFREG|S_IFSOCK)))
		err(1, "%s is not a file, device, pipe, or socket", argv[1]);
	if (S_ISREG(osb.st_mode) && !notruncate)
		if (ftruncate(ofd, 0) == -1)
			err(1, "truncate(%s)", argv[1]);

	if (oseek != 0 && lseek(ofd, oseek, SEEK_SET) == -1)
		err(1, "lseek(%s)", argv[1]);
	writeall(ofd, obuf + iseek, wlen);

	/* Explict close to force final writes to flash etc. */
	close(ofd);

	if (!quiet)
		printf("wrote %jd bytes\n", wlen);

	/* XXX: won't work if you can't map olen + oseek even with small olen */
	if (verify_write) {
		if ((ofd = open(argv[1], O_RDONLY)) == -1)
			err(1, "open(%s)", argv[1]);
		if ((vbuf = mmap(NULL, wlen + oseek, PROT_READ, MAP_PRIVATE,
		    ofd, 0)) == MAP_FAILED)
			err(1, "mmap(%s)", argv[1]);
		if (memcmp(obuf + iseek, vbuf + oseek, wlen) != 0)
			err(1, "output file does not match input!");
		if (!quiet)
			printf("verified %jd written bytes\n", wlen);
	}

	if (obuf != ibuf)
		free(obuf);

	munmap(ibuf, isb.st_size);

	return(0);
}
