/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2022 Tomohiro Kusumi <tkusumi@netbsd.org>
 * Copyright (c) 2011-2022 The DragonFly Project.  All rights reserved.
 *
 * This code is derived from software contributed to The DragonFly Project
 * by Matthew Dillon <dillon@dragonflybsd.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of The DragonFly Project nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific, prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/dirent.h>
#include <sys/uuid.h>

#include "hammer2.h"

int
hammer2_get_dtype(uint8_t type)
{
	switch (type) {
	case HAMMER2_OBJTYPE_UNKNOWN:
		return (DT_UNKNOWN);
	case HAMMER2_OBJTYPE_DIRECTORY:
		return (DT_DIR);
	case HAMMER2_OBJTYPE_REGFILE:
		return (DT_REG);
	case HAMMER2_OBJTYPE_FIFO:
		return (DT_FIFO);
	case HAMMER2_OBJTYPE_CDEV:
		return (DT_CHR);
	case HAMMER2_OBJTYPE_BDEV:
		return (DT_BLK);
	case HAMMER2_OBJTYPE_SOFTLINK:
		return (DT_LNK);
	case HAMMER2_OBJTYPE_SOCKET:
		return (DT_SOCK);
	case HAMMER2_OBJTYPE_WHITEOUT:
		return (DT_UNKNOWN);
	default:
		return (DT_UNKNOWN);
	}
	/* not reached */
}

int
hammer2_get_vtype(uint8_t type)
{
	switch (type) {
	case HAMMER2_OBJTYPE_UNKNOWN:
		return (VBAD);
	case HAMMER2_OBJTYPE_DIRECTORY:
		return (VDIR);
	case HAMMER2_OBJTYPE_REGFILE:
		return (VREG);
	case HAMMER2_OBJTYPE_FIFO:
		return (VFIFO);
	case HAMMER2_OBJTYPE_CDEV:
		return (VCHR);
	case HAMMER2_OBJTYPE_BDEV:
		return (VBLK);
	case HAMMER2_OBJTYPE_SOFTLINK:
		return (VLNK);
	case HAMMER2_OBJTYPE_SOCKET:
		return (VSOCK);
	case HAMMER2_OBJTYPE_WHITEOUT:
		return (VBAD);
	default:
		return (VBAD);
	}
	/* not reached */
}

/*
 * Convert a HAMMER2 64-bit time to a timespec.
 */
void
hammer2_time_to_timespec(uint64_t xtime, struct timespec *ts)
{
	ts->tv_sec = (unsigned long)(xtime / 1000000);
	ts->tv_nsec = (unsigned int)(xtime % 1000000) * 1000L;
}

/*
 * Convert a uuid to a unix uid or gid.
 */
uint32_t
hammer2_to_unix_xid(const struct uuid *uuid)
{
	return (*(const uint32_t *)&uuid->node[2]);
}

/*
 * Borrow HAMMER1's directory hash algorithm #1 with a few modifications.
 * The filename is split into fields which are hashed separately and then
 * added together.
 *
 * Differences include: bit 63 must be set to 1 for HAMMER2 (HAMMER1 sets
 * it to 0), this is because bit63=0 is used for hidden hardlinked inodes.
 * (This means we do not need to do a 0-check/or-with-0x100000000 either).
 *
 * Also, the iscsi crc code is used instead of the old crc32 code.
 */
hammer2_key_t
hammer2_dirhash(const unsigned char *name, size_t len)
{
	const unsigned char *aname = name;
	uint32_t crcx;
	uint64_t key;
	size_t i, j;

	key = 0;

	/* m32 */
	crcx = 0;
	for (i = j = 0; i < len; ++i) {
		if (aname[i] == '.' ||
		    aname[i] == '-' ||
		    aname[i] == '_' ||
		    aname[i] == '~') {
			if (i != j)
				crcx += hammer2_icrc32(aname + j, i - j);
			j = i + 1;
		}
	}
	if (i != j)
		crcx += hammer2_icrc32(aname + j, i - j);

	/*
	 * The directory hash utilizes the top 32 bits of the 64-bit key.
	 * Bit 63 must be set to 1.
	 */
	crcx |= 0x80000000U;
	key |= (uint64_t)crcx << 32;

	/*
	 * l16 - crc of entire filename
	 * This crc reduces degenerate hash collision conditions.
	 */
	crcx = hammer2_icrc32(aname, len);
	crcx = crcx ^ (crcx << 16);
	key |= crcx & 0xFFFF0000U;

	/*
	 * Set bit 15.  This allows readdir to strip bit 63 so a positive
	 * 64-bit cookie/offset can always be returned, and still guarantee
	 * that the values 0x0000-0x7FFF are available for artificial entries
	 * ('.' and '..').
	 */
	key |= 0x8000U;

	return (key);
}

/*
 * The logical block size is currently always PBUFSIZE.
 */
int
hammer2_calc_logical(hammer2_inode_t *ip, hammer2_off_t uoff,
    hammer2_key_t *lbasep, hammer2_key_t *leofp)
{
	if (lbasep)
		*lbasep = uoff & ~HAMMER2_PBUFMASK64;
	if (leofp)
		*leofp = (ip->meta.size + HAMMER2_PBUFMASK64) &
		    ~HAMMER2_PBUFMASK64;

	return (HAMMER2_PBUFSIZE);
}

int
hammer2_get_logical(void)
{
	return (hammer2_calc_logical(NULL, 0, NULL, NULL));
}

const char *
hammer2_breftype_to_str(uint8_t type)
{
	switch (type) {
	case HAMMER2_BREF_TYPE_EMPTY:
		return ("empty");
	case HAMMER2_BREF_TYPE_INODE:
		return ("inode");
	case HAMMER2_BREF_TYPE_INDIRECT:
		return ("indirect");
	case HAMMER2_BREF_TYPE_DATA:
		return ("data");
	case HAMMER2_BREF_TYPE_DIRENT:
		return ("dirent");
	case HAMMER2_BREF_TYPE_FREEMAP_NODE:
		return ("freemap_node");
	case HAMMER2_BREF_TYPE_FREEMAP_LEAF:
		return ("freemap_leaf");
	case HAMMER2_BREF_TYPE_INVALID:
		return ("invalid");
	case HAMMER2_BREF_TYPE_FREEMAP:
		return ("freemap");
	case HAMMER2_BREF_TYPE_VOLUME:
		return ("volume");
	default:
		return ("unknown");
	}
}
