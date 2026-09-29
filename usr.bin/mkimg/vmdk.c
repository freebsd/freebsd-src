/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2014 Juniper Networks, Inc.
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
#include <sys/errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#include "endian.h"
#include "image.h"
#include "format.h"
#include "mkimg.h"

#define	VMDK_IMAGE_ROUND	1048576
#define	VMDK_MIN_GRAIN_SIZE	8192
#define	VMDK_SECTOR_SIZE	512
#define	VMDK_STREAM_GRAIN_SIZE	65536

struct vmdk_header {
	uint32_t	magic;
#define	VMDK_MAGIC		0x564d444b
	uint32_t	version;
#define	VMDK_VERSION		1
#define	VMDK_VERSION_STREAM	3
	uint32_t	flags;
#define	VMDK_FLAGS_NL_TEST	(1 << 0)
#define	VMDK_FLAGS_RGT_USED	(1 << 1)
#define	VMDK_FLAGS_COMPRESSED	(1 << 16)
#define	VMDK_FLAGS_MARKERS	(1 << 17)
	uint64_t	capacity;
	uint64_t	grain_size;
	uint64_t	desc_offset;
	uint64_t	desc_size;
	uint32_t	ngtes;
#define	VMDK_NGTES		512
	uint64_t	rgd_offset;
	uint64_t	gd_offset;
#define	VMDK_GD_AT_END		0xffffffffffffffffULL
	uint64_t	overhead;
	uint8_t		unclean;
	uint32_t	nl_test;
#define	VMDK_NL_TEST		0x0a200d0a
	uint16_t	compress;
#define	VMDK_COMPRESS_NONE	0
#define	VMDK_COMPRESS_DEFLATE	1
	char		padding[433];
} __attribute__((__packed__));

/*
 * In a stream-optimized VMDK, each grain is preceded by a 12-byte marker
 * giving the grain's LBA and compressed size, and metadata (grain tables,
 * the grain directory, and the footer) is preceded by a sector-sized
 * marker giving the size of the metadata in sectors and its type.  A
 * sector of zeroes marks the end of the stream.
 */
#define	VMDK_GRAIN_MARKER_SIZE	12

struct vmdk_marker {
	uint64_t	val;
	uint32_t	size;
} __attribute__((__packed__));
_Static_assert(sizeof(struct vmdk_marker) == VMDK_GRAIN_MARKER_SIZE,
    "Wrong size for VMDK marker");

struct vmdk_metadata_marker {
	uint64_t	val;
	uint32_t	size;
	uint32_t	type;
#define	VMDK_MARKER_EOS		0
#define	VMDK_MARKER_GT		1
#define	VMDK_MARKER_GD		2
#define	VMDK_MARKER_FOOTER	3
	char		padding[496];
} __attribute__((__packed__));
_Static_assert(sizeof(struct vmdk_metadata_marker) == VMDK_SECTOR_SIZE,
    "Wrong size for VMDK marker");

static const char desc_fmt[] =
    "# Disk DescriptorFile\n"
    "version=%d\n"
    "CID=%08x\n"
    "parentCID=ffffffff\n"
    "createType=\"%s\"\n"
    "# Extent description\n"
    "RW %ju SPARSE \"%s\"\n"
    "# The Disk Data Base\n"
    "#DDB\n"
    "ddb.adapterType = \"ide\"\n"
    "ddb.geometry.cylinders = \"%u\"\n"
    "ddb.geometry.heads = \"%u\"\n"
    "ddb.geometry.sectors = \"%u\"\n"
    "%s";

static uint64_t grainsz;

static int
vmdk_resize(lba_t imgsz)
{
	uint64_t imagesz;

	imagesz = imgsz * secsz;
	imagesz = (imagesz + VMDK_IMAGE_ROUND - 1) & ~(VMDK_IMAGE_ROUND - 1);
	grainsz = (blksz < VMDK_MIN_GRAIN_SIZE) ? VMDK_MIN_GRAIN_SIZE : blksz;

	if (verbose)
		fprintf(stderr, "VMDK: image size = %ju, grain size = %ju\n",
		    (uintmax_t)imagesz, (uintmax_t)grainsz);

	grainsz /= VMDK_SECTOR_SIZE;
	return (image_set_size(imagesz / secsz));
}

static int
vmdk_write(int fd)
{
	struct vmdk_header hdr;
	uint32_t *gt, *gd, *rgd;
	char *buf, *desc;
	off_t cur, lim;
	uint64_t imagesz;
	lba_t blkofs, blkcnt;
	size_t gdsz, gtsz;
	uint32_t sec, cursec;
	int error, desc_len, n, ngrains, ngts;

	imagesz = (image_get_size() * secsz) / VMDK_SECTOR_SIZE;

	memset(&hdr, 0, sizeof(hdr));
	le32enc(&hdr.magic, VMDK_MAGIC);
	le32enc(&hdr.version, VMDK_VERSION);
	le32enc(&hdr.flags, VMDK_FLAGS_NL_TEST | VMDK_FLAGS_RGT_USED);
	le64enc(&hdr.capacity, imagesz);
	le64enc(&hdr.grain_size, grainsz);

	n = asprintf(&desc, desc_fmt, 1 /*version*/, 0 /*CID*/,
	    "monolithicSparse" /*type*/,
	    (uintmax_t)imagesz /*size*/, "" /*name*/,
	    ncyls /*cylinders*/, nheads /*heads*/, nsecs /*sectors*/,
	    "" /*extra*/);
	if (n == -1)
		return (ENOMEM);

	desc_len = (n + VMDK_SECTOR_SIZE - 1) & ~(VMDK_SECTOR_SIZE - 1);
	buf = realloc(desc, desc_len);
	if (buf == NULL) {
		free(desc);
		return (ENOMEM);
	}
	desc = buf;
	memset(desc + n, 0, desc_len - n);

	le64enc(&hdr.desc_offset, 1);
	le64enc(&hdr.desc_size, desc_len / VMDK_SECTOR_SIZE);
	le32enc(&hdr.ngtes, VMDK_NGTES);

	sec = desc_len / VMDK_SECTOR_SIZE + 1;

	ngrains = imagesz / grainsz;
	ngts = (ngrains + VMDK_NGTES - 1) / VMDK_NGTES;
	gdsz = (ngts * sizeof(uint32_t) + VMDK_SECTOR_SIZE - 1) &
	    ~(VMDK_SECTOR_SIZE - 1);

	gd = calloc(1, gdsz);
	if (gd == NULL) {
		free(desc);
		return (ENOMEM);
	}
	le64enc(&hdr.gd_offset, sec);
	sec += gdsz / VMDK_SECTOR_SIZE;
	for (n = 0; n < ngts; n++) {
		le32enc(gd + n, sec);
		sec += VMDK_NGTES * sizeof(uint32_t) / VMDK_SECTOR_SIZE;
	}

	rgd = calloc(1, gdsz);
	if (rgd == NULL) {
		free(gd);
		free(desc);
		return (ENOMEM);
	}
	le64enc(&hdr.rgd_offset, sec);
	sec += gdsz / VMDK_SECTOR_SIZE;
	for (n = 0; n < ngts; n++) {
		le32enc(rgd + n, sec);
		sec += VMDK_NGTES * sizeof(uint32_t) / VMDK_SECTOR_SIZE;
	}

	sec = (sec + grainsz - 1) & ~(grainsz - 1);

	if (verbose)
		fprintf(stderr, "VMDK: overhead = %ju\n",
		    (uintmax_t)(sec * VMDK_SECTOR_SIZE));

	le64enc(&hdr.overhead, sec);
	be32enc(&hdr.nl_test, VMDK_NL_TEST);

	gt = calloc(ngts, VMDK_NGTES * sizeof(uint32_t));
	if (gt == NULL) {
		free(rgd);
		free(gd);
		free(desc);
		return (ENOMEM);
	}
	gtsz = ngts * VMDK_NGTES * sizeof(uint32_t);

	cursec = sec;
	blkcnt = (grainsz * VMDK_SECTOR_SIZE) / secsz;
	for (n = 0; n < ngrains; n++) {
		blkofs = n * blkcnt;
		if (image_data(blkofs, blkcnt)) {
			le32enc(gt + n, cursec);
			cursec += grainsz;
		}
	}

	error = 0;
	if (!error && sparse_write(fd, &hdr, VMDK_SECTOR_SIZE) < 0)
		error = errno;
	if (!error && sparse_write(fd, desc, desc_len) < 0)
		error = errno;
	if (!error && sparse_write(fd, gd, gdsz) < 0)
		error = errno;
	if (!error && sparse_write(fd, gt, gtsz) < 0)
		error = errno;
	if (!error && sparse_write(fd, rgd, gdsz) < 0)
		error = errno;
	if (!error && sparse_write(fd, gt, gtsz) < 0)
		error = errno;
	free(gt);
	free(rgd);
	free(gd);
	free(desc);
	if (error)
		return (error);

	cur = VMDK_SECTOR_SIZE + desc_len + (gdsz + gtsz) * 2;
	lim = sec * VMDK_SECTOR_SIZE;
	if (cur < lim) {
		buf = calloc(1, VMDK_SECTOR_SIZE);
		if (buf == NULL)
			error = ENOMEM;
		while (!error && cur < lim) {
			if (sparse_write(fd, buf, VMDK_SECTOR_SIZE) < 0)
				error = errno;
			cur += VMDK_SECTOR_SIZE;
		}
		if (buf != NULL)
			free(buf);
	}
	if (error)
		return (error);

	blkcnt = (grainsz * VMDK_SECTOR_SIZE) / secsz;
	for (n = 0; n < ngrains; n++) {
		blkofs = n * blkcnt;
		if (image_data(blkofs, blkcnt)) {
			error = image_copyout_region(fd, blkofs, blkcnt);
			if (error)
				return (error);
		}
	}
	return (image_copyout_done(fd));
}

static struct mkimg_format vmdk_format = {
	.name = "vmdk",
	.description = "Virtual Machine Disk",
	.resize = vmdk_resize,
	.write = vmdk_write,
};

FORMAT_DEFINE(vmdk_format);

/*
 * Stream-optimized VMDK: grains are compressed and written sequentially,
 * each grain table is written after the grains it describes, and the grain
 * directory and a footer containing a copy of the header (with the grain
 * directory offset filled in) come at the end.
 */

static int
vmdk_stream_resize(lba_t imgsz)
{
	uint64_t imagesz;

	imagesz = imgsz * secsz;
	imagesz = (imagesz + VMDK_IMAGE_ROUND - 1) & ~(VMDK_IMAGE_ROUND - 1);
	grainsz = (secsz < VMDK_STREAM_GRAIN_SIZE) ?
	    VMDK_STREAM_GRAIN_SIZE : secsz;

	if (verbose)
		fprintf(stderr, "VMDK: image size = %ju, grain size = %ju\n",
		    (uintmax_t)imagesz, (uintmax_t)grainsz);

	grainsz /= VMDK_SECTOR_SIZE;
	return (image_set_size(imagesz / secsz));
}

static int
vmdk_stream_marker(int fd, uint64_t val, uint32_t type)
{
	struct vmdk_metadata_marker m;

	memset(&m, 0, sizeof(m));
	le64enc(&m.val, val);
	le32enc(&m.type, type);
	if (sparse_write(fd, &m, sizeof(m)) < 0)
		return (errno);
	return (0);
}

/*
 * Write out the grain table gt, preceded by a marker, starting at sector
 * *secp; record its location in the grain directory entry *gde, advance
 * *secp past it, and clear gt for reuse.
 */
static int
vmdk_stream_gt(int fd, uint32_t *gt, uint32_t *gde, uint64_t *secp)
{
	size_t gtsz;
	int error;

	gtsz = VMDK_NGTES * sizeof(uint32_t);
	error = vmdk_stream_marker(fd, gtsz / VMDK_SECTOR_SIZE,
	    VMDK_MARKER_GT);
	if (error)
		return (error);
	*secp += 1;

	/* Grain directory entries are 32-bit sector numbers. */
	if (*secp > UINT32_MAX)
		return (EFBIG);
	le32enc(gde, *secp);
	if (sparse_write(fd, gt, gtsz) < 0)
		return (errno);
	*secp += gtsz / VMDK_SECTOR_SIZE;
	memset(gt, 0, gtsz);
	return (0);
}

static int
vmdk_stream_iszero(const uint8_t *buf, size_t len)
{
	size_t i;

	for (i = 0; i < len; i++) {
		if (buf[i] != 0)
			return (0);
	}
	return (1);
}

static int
vmdk_stream_write(int fd)
{
	struct vmdk_header hdr;
	struct vmdk_marker ghead;
	uint32_t *gd, *gt;
	uint8_t *gbuf, *zbuf;
	char *buf, *desc;
	uint64_t gdofs, grain, imagesz, ngrains, ngts, overhead, sec;
	lba_t blkcnt;
	size_t gdsz, gtsz, grainbytes, pos, len, zbufsz;
	uLongf zlen;
	int desc_len, error, gtused, n, zerror;

	imagesz = (image_get_size() * secsz) / VMDK_SECTOR_SIZE;
	grainbytes = grainsz * VMDK_SECTOR_SIZE;
	blkcnt = grainbytes / secsz;
	ngrains = imagesz / grainsz;
	ngts = (ngrains + VMDK_NGTES - 1) / VMDK_NGTES;
	gdsz = (ngts * sizeof(uint32_t) + VMDK_SECTOR_SIZE - 1) &
	    ~(VMDK_SECTOR_SIZE - 1);
	gtsz = VMDK_NGTES * sizeof(uint32_t);

	n = asprintf(&desc, desc_fmt, 1 /*version*/, 0 /*CID*/,
	    "streamOptimized" /*type*/, (uintmax_t)imagesz /*size*/,
	    "" /*name*/, ncyls /*cylinders*/, nheads /*heads*/,
	    nsecs /*sectors*/, "ddb.virtualHWVersion = \"4\"\n" /*extra*/);
	if (n == -1)
		return (ENOMEM);

	desc_len = (n + VMDK_SECTOR_SIZE - 1) & ~(VMDK_SECTOR_SIZE - 1);
	buf = realloc(desc, desc_len);
	if (buf == NULL) {
		free(desc);
		return (ENOMEM);
	}
	desc = buf;
	memset(desc + n, 0, desc_len - n);

	/* Grains start at the first grain boundary after the descriptor. */
	overhead = 1 + desc_len / VMDK_SECTOR_SIZE;
	overhead = (overhead + grainsz - 1) & ~(grainsz - 1);

	memset(&hdr, 0, sizeof(hdr));
	le32enc(&hdr.magic, VMDK_MAGIC);
	le32enc(&hdr.version, VMDK_VERSION_STREAM);
	le32enc(&hdr.flags, VMDK_FLAGS_NL_TEST | VMDK_FLAGS_COMPRESSED |
	    VMDK_FLAGS_MARKERS);
	le64enc(&hdr.capacity, imagesz);
	le64enc(&hdr.grain_size, grainsz);
	le64enc(&hdr.desc_offset, 1);
	le64enc(&hdr.desc_size, desc_len / VMDK_SECTOR_SIZE);
	le32enc(&hdr.ngtes, VMDK_NGTES);
	le64enc(&hdr.gd_offset, VMDK_GD_AT_END);
	le64enc(&hdr.overhead, overhead);
	be32enc(&hdr.nl_test, VMDK_NL_TEST);
	le16enc(&hdr.compress, VMDK_COMPRESS_DEFLATE);

	if (verbose)
		fprintf(stderr, "VMDK: overhead = %ju\n",
		    (uintmax_t)(overhead * VMDK_SECTOR_SIZE));

	gd = calloc(1, gdsz);
	gt = calloc(1, gtsz);
	gbuf = malloc(grainbytes);
	zbufsz = (VMDK_GRAIN_MARKER_SIZE + compressBound(grainbytes) +
	    VMDK_SECTOR_SIZE - 1) & ~(VMDK_SECTOR_SIZE - 1);
	zbuf = malloc(zbufsz);
	if (gd == NULL || gt == NULL || gbuf == NULL || zbuf == NULL) {
		error = ENOMEM;
		goto out;
	}

	/* Write the header and descriptor, and pad to the first grain. */
	error = 0;
	if (sparse_write(fd, &hdr, VMDK_SECTOR_SIZE) < 0 ||
	    sparse_write(fd, desc, desc_len) < 0) {
		error = errno;
		goto out;
	}
	error = image_copyout_zeroes(fd,
	    (overhead - 1) * VMDK_SECTOR_SIZE - desc_len);
	if (error)
		goto out;
	sec = overhead;

	gtused = 0;
	for (grain = 0; grain < ngrains; grain++) {
		/*
		 * If we're starting a new grain table, write out the
		 * previous one (but only if it has any entries).
		 */
		if (grain % VMDK_NGTES == 0 && gtused) {
			error = vmdk_stream_gt(fd, gt,
			    gd + grain / VMDK_NGTES - 1, &sec);
			if (error)
				goto out;
			gtused = 0;
		}

		/* Skip grains which are entirely zero. */
		if (!image_data(grain * blkcnt, blkcnt))
			continue;
		error = image_buffer_region((char *)gbuf, grain * blkcnt,
		    blkcnt);
		if (error)
			goto out;
		if (vmdk_stream_iszero(gbuf, grainbytes))
			continue;

		/* Compress the grain, after space for the grain marker. */
		zlen = zbufsz - VMDK_GRAIN_MARKER_SIZE;
		zerror = compress2(zbuf + VMDK_GRAIN_MARKER_SIZE, &zlen,
		    gbuf, grainbytes, Z_DEFAULT_COMPRESSION);
		if (zerror != Z_OK) {
			error = (zerror == Z_MEM_ERROR) ? ENOMEM : EIO;
			goto out;
		}

		/* Fill in the marker, and pad to a sector boundary. */
		le64enc(&ghead.val, grain * grainsz);
		le32enc(&ghead.size, zlen);
		memcpy(&zbuf[0], &ghead, VMDK_GRAIN_MARKER_SIZE);
		pos = VMDK_GRAIN_MARKER_SIZE + zlen;
		len = (pos + VMDK_SECTOR_SIZE - 1) & ~(VMDK_SECTOR_SIZE - 1);
		memset(&zbuf[pos], 0, len - pos);

		/* Grain table entries are 32-bit sector numbers. */
		if (sec > UINT32_MAX) {
			error = EFBIG;
			goto out;
		}

		/* Record the grain's location and write it out. */
		le32enc(gt + grain % VMDK_NGTES, sec);
		if (sparse_write(fd, zbuf, len) < 0) {
			error = errno;
			goto out;
		}
		sec += len / VMDK_SECTOR_SIZE;
		gtused = 1;
	}

	/* Write out the final grain table, if it has any entries. */
	if (gtused) {
		error = vmdk_stream_gt(fd, gt, gd + (ngrains - 1) / VMDK_NGTES,
		    &sec);
		if (error)
			goto out;
	}

	/* Write the grain directory. */
	error = vmdk_stream_marker(fd, gdsz / VMDK_SECTOR_SIZE,
	    VMDK_MARKER_GD);
	if (error)
		goto out;
	gdofs = sec + 1;
	if (sparse_write(fd, gd, gdsz) < 0) {
		error = errno;
		goto out;
	}

	/* Write the footer: a copy of the header with the GD offset. */
	error = vmdk_stream_marker(fd, 1, VMDK_MARKER_FOOTER);
	if (error)
		goto out;
	le64enc(&hdr.gd_offset, gdofs);
	if (sparse_write(fd, &hdr, VMDK_SECTOR_SIZE) < 0) {
		error = errno;
		goto out;
	}

	/* Write the end-of-stream marker. */
	error = vmdk_stream_marker(fd, 0, VMDK_MARKER_EOS);
	if (error)
		goto out;

	error = image_copyout_done(fd);

out:
	free(zbuf);
	free(gbuf);
	free(gt);
	free(gd);
	free(desc);
	return (error);
}

static struct mkimg_format vmdk_stream_format = {
	.name = "vmdks",
	.description = "Virtual Machine Disk, stream-optimized (compressed)",
	.resize = vmdk_stream_resize,
	.write = vmdk_stream_write,
};

FORMAT_DEFINE(vmdk_stream_format);
