/*-
 * Copyright (c) 2014 Marcel Moolenaar
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

#include <sys/types.h>
#include <sys/endian.h>
#include <sys/errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "image.h"
#include "format.h"
#include "mkimg.h"

#undef	QCOW_SUPPORT_QCOW2

/* Default cluster sizes. */
#define	QCOW1_CLSTR_LOG2SZ	12	/* 4KB */
#define	QCOW2_CLSTR_LOG2SZ	16	/* 64KB */

struct qcow_header {
	uint32_t	magic;
#define	QCOW_MAGIC		0x514649fb
	uint32_t	version;
#define	QCOW_VERSION_1		1
#define	QCOW_VERSION_2		2
	uint64_t	path_offset;
	uint32_t	path_length;
	uint32_t	clstr_log2sz;	/* v2 only */
	uint64_t	disk_size;
	union {
		struct {
			uint8_t		clstr_log2sz;
			uint8_t		l2_log2sz;
			uint16_t	_pad;
			uint32_t	encryption;
			uint64_t	l1_offset;
		} v1;
		struct {
			uint32_t	encryption;
			uint32_t	l1_entries;
			uint64_t	l1_offset;
			uint64_t	refcnt_offset;
			uint32_t	refcnt_entries;
			uint32_t	snapshot_count;
			uint64_t	snapshot_offset;
		} v2;
	} u;
};

static u_int clstr_log2sz;

static uint64_t
round_clstr(uint64_t ofs)
{
	uint64_t clstrsz;

	clstrsz = 1UL << clstr_log2sz;
	return ((ofs + clstrsz - 1) & ~(clstrsz - 1));
}

static int
qcow_resize(lba_t imgsz, u_int version)
{
	uint64_t clstrsz, imagesz;

	switch (version) {
	case QCOW_VERSION_1:
		clstr_log2sz = QCOW1_CLSTR_LOG2SZ;
		break;
	case QCOW_VERSION_2:
		clstr_log2sz = QCOW2_CLSTR_LOG2SZ;
		break;
	default:
		return (EDOOFUS);
	}

	clstrsz = 1UL << clstr_log2sz;
	imagesz = round_clstr(imgsz * secsz);

	if (verbose)
		fprintf(stderr, "QCOW: image size = %ju, cluster size = %ju\n",
		    (uintmax_t)imagesz, (uintmax_t)clstrsz);

	return (image_set_size(imagesz / secsz));
}

static int
qcow1_resize(lba_t imgsz)
{

	return (qcow_resize(imgsz, QCOW_VERSION_1));
}

#ifdef QCOW_SUPPORT_QCOW2
static int
qcow2_resize(lba_t imgsz)
{

	return (qcow_resize(imgsz, QCOW_VERSION_2));
}
#endif

static int
qcow_write(int fd, u_int version)
{
	struct qcow_header *hdr;
	uint64_t *l1tbl, *l2tbl;
	uint16_t *rctbl;
	uint64_t n, clstrsz, imagesz, nclstrs;
	uint64_t l1ofs, l2ofs, ofs, rcofs;
	lba_t blk, blkofs, blkcnt, imgsz;
	u_int l1idx, l2idx, l2clstrs;
	int error;

	if (clstr_log2sz == 0)
		return (EDOOFUS);

	clstrsz = 1UL << clstr_log2sz;
	blkcnt = clstrsz / secsz;
	imgsz = image_get_size();
	imagesz = imgsz * secsz;
	nclstrs = imagesz >> clstr_log2sz;
	l2clstrs = (nclstrs * 8 + clstrsz - 1) > clstr_log2sz;

	l1ofs = clstrsz;
	rcofs = round_clstr(l1ofs + l2clstrs * 8);

	hdr = calloc(1, clstrsz);
	if (hdr == NULL)
		return (errno);

	be32enc(&hdr->magic, QCOW_MAGIC);
	be32enc(&hdr->version, version);
	be64enc(&hdr->disk_size, imagesz);
	switch (version) {
	case QCOW_VERSION_1:
		l2ofs = rcofs;	/* No reference counting. */
		hdr->u.v1.clstr_log2sz = clstr_log2sz;
		hdr->u.v1.l2_log2sz = clstr_log2sz - 3;
		be64enc(&hdr->u.v1.l1_offset, l1ofs);
		break;
	case QCOW_VERSION_2:
		l2ofs = round_clstr(rcofs + (nclstrs + l2clstrs) * 2);
		be32enc(&hdr->clstr_log2sz, clstr_log2sz);
		be32enc(&hdr->u.v2.l1_entries, l2clstrs);
		be64enc(&hdr->u.v2.l1_offset, l1ofs);
		be64enc(&hdr->u.v2.refcnt_offset, rcofs);
		be32enc(&hdr->u.v2.refcnt_entries, l2clstrs);
		break;
	default:
		return (EDOOFUS);
	}

	l2tbl = l1tbl = NULL;
	rctbl = NULL;

	l1tbl = calloc(1, (size_t)(rcofs - l1ofs));
	if (l1tbl == NULL) {
		error = ENOMEM;
		goto out;
	}
	if (l2ofs != rcofs) {
		rctbl = calloc(1, (size_t)(l2ofs - rcofs));
		if (rctbl == NULL) {
			error = ENOMEM;
			goto out;
		}
	}

	ofs = l2ofs;
	for (n = 0; n < nclstrs; n++) {
		l1idx = n >> (clstr_log2sz - 3);
		if (l1tbl[l1idx] != 0UL)
			continue;
		blk = n * blkcnt;
		if (image_data(blk, blkcnt)) {
			be64enc(l1tbl + l1idx, ofs);
			ofs += clstrsz;
		}
	}

	error = 0;
	if (!error && sparse_write(fd, hdr, clstrsz) < 0)
		error = errno;
	if (!error && sparse_write(fd, l1tbl, (size_t)(rcofs - l1ofs)) < 0)
		error = errno;
	/* XXX refcnt table. */
	if (error)
		goto out;

	free(hdr);
	hdr = NULL;
	if (rctbl != NULL) {
		free(rctbl);
		rctbl = NULL;
	}

	l2tbl = malloc(clstrsz);
	if (l2tbl == NULL) {
		error = ENOMEM;
		goto out;
	}

	for (l1idx = 0; l1idx < l2clstrs; l1idx++) {
		if (l1tbl[l1idx] == 0)
			continue;
		memset(l2tbl, 0, clstrsz);
		blkofs = (lba_t)l1idx * (clstrsz * (clstrsz >> 3));
		for (l2idx = 0; l2idx < (clstrsz >> 3); l2idx++) {
			blk = blkofs + (lba_t)l2idx * blkcnt;
			if (blk >= imgsz)
				break;
			if (image_data(blk, blkcnt)) {
				be64enc(l2tbl + l2idx, ofs);
				ofs += clstrsz;
			}
		}
		if (sparse_write(fd, l2tbl, clstrsz) < 0) {
			error = errno;
			goto out;
		}
	}

	free(l2tbl);
	l2tbl = NULL;
	free(l1tbl);
	l1tbl = NULL;

	error = 0;
	for (n = 0; n < nclstrs; n++) {
		blk = n * blkcnt;
		if (image_data(blk, blkcnt)) {
			error = image_copyout_region(fd, blk, blkcnt);
			if (error)
				break;
		}
	}
	if (!error)
		error = image_copyout_done(fd);

 out:
	if (l2tbl != NULL)
		free(l2tbl);
	if (rctbl != NULL)
		free(rctbl);
	if (l1tbl != NULL)
		free(l1tbl);
	if (hdr != NULL)
		free(hdr);
	return (error);
}

static int
qcow1_write(int fd)
{

	return (qcow_write(fd, QCOW_VERSION_1));
}

#ifdef QCOW_SUPPORT_QCOW2
static int
qcow2_write(int fd)
{

	return (qcow_write(fd, QCOW_VERSION_2));
}
#endif

static struct mkimg_format qcow1_format = {
	.name = "qcow",
	.description = "QEMU Copy-On-Write, version 1",
	.resize = qcow1_resize,
	.write = qcow1_write,
};
FORMAT_DEFINE(qcow1_format);

#ifdef QCOW_SUPPORT_QCOW2
static struct mkimg_format qcow2_format = {
	.name = "qcow2",
	.description = "QEMU Copy-On-Write, version 2",
	.resize = qcow2_resize,
	.write = qcow2_write,
};
FORMAT_DEFINE(qcow2_format);
#endif
