/*-
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
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/apm.h>
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

struct vmdk_header {
	uint32_t	magic;
#define	VMDK_MAGIC		0x564d444b
	uint32_t	version;
#define	VMDK_VERSION		1
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
	uint64_t	overhead;
	uint8_t		unclean;
	uint8_t		nl_test[4];
#define	VMDK_NL_TEST		0x0a200d0a
	uint16_t	compress;
#define	VMDK_COMPRESS_NONE	0
#define	VMDK_COMPRESS_DEFLATE	1
	char		padding[433];
} __attribute__((__packed__));

static const char desc_fmt[] =
    "# Disk DescriptorFile\n"
    "version=%d\n"
    "CID=%08x\n"
    "parentCID=ffffffff\n"
    "createType=\"monolithicSparse\"\n"
    "# Extent description\n"
    "RW %ju SPARSE \"%s\"\n"
    "# The Disk Data Base\n"
    "#DDB\n"
    "ddb.adapterType = \"ide\"\n"
    "ddb.geometry.cylinders = \"%u\"\n"
    "ddb.geometry.heads = \"%u\"\n"
    "ddb.geometry.sectors = \"%u\"\n";

static int
vmdk_resize(lba_t imgsz __unused)
{

	/*
	 * Caulculate optimal grain size and round image size to
	 * a multiple of the grain size.
	 */
	return (ENOSYS);
}

static int
vmdk_write(int fd __unused)
{
	char *desc;
	lba_t imgsz;
	int desc_len;

	imgsz = image_get_size();
	desc_len = asprintf(&desc, desc_fmt, 1 /*version*/, 0 /*CID*/,
	    (uintmax_t)imgsz /*size*/, "mkimg.vmdk" /*name*/,
	    ncyls /*cylinders*/, nheads /*heads*/, nsecs /*sectors*/);
	desc_len = (desc_len + 512 - 1) & ~(512 - 1);
	desc = realloc(desc, desc_len);

	/*
	 * Steps:
	 * 1. create embedded descriptor. We need to know its size upfront.
	 * 2. create and populate grain directory and tables. This means
	 *    iterating over the written sectors of the image.
	 * 3. (optional) create and populate redundant directory and
	 *    tables while doing step 2.
	 * 4. create and write header (512 bytes)
	 * 5. write descriptor (# x 512 bytes)
	 * 6. write grain directory and tables (# x 512 bytes)
	 * 7. (optional) write redundant directory and tables (# x 512 bytes)
	 * 8. align to grain size.
	 * 9. create and write grains.
	 *
	 * Notes:
	 * 1. The drain directory is being ignored by some implementations
	 *    so the tables must be at their known/assumed offsets.
	 * 2. Default grain size is 128 sectors (= 64KB).
	 * 3. There are 512 entries in a table, each entry being 32-bits.
	 *    Thus, a grain table is 2KB (= 4 sectors).
	 * 4. Each grain table covers 512 * 128 sectors (= 64K sectors).
	 *    With 512-bytes per sector, this yields 32MB of disk data.
	 * 5. For smaller images, the grain size can be reduced to avoid
	 *    rounding the output file to 32MB. The minimum grain size is
	 *    8 sectors (= 4KB). The smallest VMDK file is 2MB without
	 *    overhead (= metadata).
	 * 6. The capacity is a multiple of the grain size.
	 */
	return (ENOSYS);
}

static struct mkimg_format vmdk_format = {
	.name = "vmdk",
	.description = "Virtual Machine Disk",
	.resize = vmdk_resize,
	.write = vmdk_write,
};

FORMAT_DEFINE(vmdk_format);
