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
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <uuid.h>

#include "image.h"
#include "format.h"
#include "mkimg.h"

/*
 * Notes:
 * o   File is in network byte order.
 * o   File layout:
 *	copy of disk footer
 *	dynamic disk header
 *	block allocation table (BAT)
 *	data blocks
 *	disk footer
 * o   The timestamp is seconds since 1/1/2000 12:00:00 AM UTC
 */

#define	VHD_BLOCK_SIZE	4096	/* 2MB blocks */

struct vhd_footer {
	char		cookie[8];
#define	VHD_COOKIE_MICROSOFT	"conectix"
#define	VHD_COOKIE_FREEBSD	"FreeBSD"
	uint32_t	features;
#define	VHD_FEATURES_TEMPORARY	0x01
#define	VHD_FEATURES_RESERVED	0x02
	uint32_t	version;
#define	VHD_VERSION		0x00010000
	uint64_t	data_offset;
	uint32_t	timestamp;
	char		creator_tool;
#define	VHD_CREATOR_TOOL_MS_VPC	"vpc "		/* Virtual PC */
#define	VHD_CREATOR_TOOL_MS_VS	"vs  "		/* Virtual Server */
#define	VHD_CREATOR_TOOL_FBSD	"mkim"		/* FreeBSD mkimg */
	uint32_t	creator_version;
#define	VHD_CREATOR_VERS_MS_VPC	0x00050000
#define	VHD_CREATOR_VERS_MS_VS	0x00010000
#define	VHD_CREATOR_VERS_FBSD	0x00010000
	char		creator_os[4];
#define	VHD_CREATOR_OS_WINDOWS	"Wi2k"
#define	VHD_CREATOR_OS_MAC	"Mac "
#define	VHD_CREATOR_OS_FREEBSD	"FBSD"
	uint64_t	original_size;
	uint64_t	current_size;
	uint16_t	cylinders;
	uint8_t		heads;
	uint8_t		sectors;
	uint32_t	disk_type;
#define	VHD_DISK_TYPE_FIXED	2
#define	VHD_DISK_TYPE_DYNAMIC	3
#define	VHD_DISK_TYPE_DIFF	4
	uint32_t	checksum;
	uuid_t		id;
	uint8_t		saved_state;
	uint8_t		_reserved[427];
};
_Static_assert(sizeof(struct vhd_footer) == 512, "Wrong size for footer");

struct vhd_dyn_header {
	uint64_t	cookie;
	uint64_t	data_offset;
	uint64_t	table_offset;
	uint32_t	version;
	uint32_t	max_entries;
	uint32_t	block_size;
	uint32_t	checksum;
	uuid_t		parent_id;
	uint32_t	parent_timestamp;
	char		_reserved1[4];
	uint16_t	parent_name[256];	/* UTF-16 */
	struct {
		uint32_t	code;
		uint32_t	data_space;
		uint32_t	data_length;
		uint32_t	_reserved;
		uint64_t	data_offset;
	} parent_locator[8];
	char		_reserved2[256];
};
_Static_assert(sizeof(struct vhd_dyn_header) == 1024, "Wrong size for header");

static int
vhd_resize(lba_t imgsz)
{

	/* Round to a multiple of the block size. */
	imgsz = (imgsz + VHD_BLOCK_SIZE - 1) & ~(VHD_BLOCK_SIZE - 1);
	return (image_set_size(imgsz));
}

static int
vhd_write(int fd)
{

	return (image_copyout(fd));
}

static struct mkimg_format vhd_format = {
	.name = "vhd",
	.description = "Virtual Hard Disk",
	.resize = vhd_resize,
	.write = vhd_write,
};

FORMAT_DEFINE(vhd_format);
