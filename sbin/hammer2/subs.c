/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2022 Tomohiro Kusumi <tkusumi@netbsd.org>
 * Copyright (c) 2011-2012 The DragonFly Project.  All rights reserved.
 *
 * This code is derived from software contributed to The DragonFly Project
 * by Matthew Dillon <dillon@dragonflybsd.org>
 * by Venkatesh Srinivas <vsrinivas@dragonflybsd.org>
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

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/statvfs.h>
#include <sys/disk.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <err.h>
#include <uuid.h>

#include <fs/hammer2/hammer2_disk.h>
#include <fs/hammer2/hammer2_ioctl.h>

#include "hammer2_subs.h"

/*
 * Obtain a file descriptor that the caller can execute ioctl()'s on.
 */
int
hammer2_ioctl_handle(const char *sel_path)
{
	struct hammer2_ioc_version info;
	int fd;

	if (sel_path == NULL)
		sel_path = ".";

	fd = open(sel_path, O_RDONLY, 0);
	if (fd < 0) {
		fprintf(stderr, "hammer2: Unable to open %s: %s\n",
			sel_path, strerror(errno));
		return(-1);
	}
	if (ioctl(fd, HAMMER2IOC_VERSION_GET, &info) < 0) {
		fprintf(stderr, "hammer2: '%s' is not a hammer2 filesystem\n",
			sel_path);
		close(fd);
		return(-1);
	}
	return (fd);
}

const char *
hammer2_time64_to_str(uint64_t htime64, char **strp)
{
	struct tm *tp;
	time_t t;

	if (*strp) {
		free(*strp);
		*strp = NULL;
	}
	*strp = malloc(64);
	t = htime64 / 1000000;
	tp = localtime(&t);
	strftime(*strp, 64, "%d-%b-%Y %H:%M:%S", tp);
	return (*strp);
}

const char *
hammer2_uuid_to_str(const uuid_t *uuid, char **strp)
{
	uint32_t status;
	if (*strp) {
		free(*strp);
		*strp = NULL;
	}
	uuid_to_string(uuid, strp, &status);
	return (*strp);
}

const char *
hammer2_iptype_to_str(uint8_t type)
{
	switch(type) {
	case HAMMER2_OBJTYPE_UNKNOWN:
		return("UNKNOWN");
	case HAMMER2_OBJTYPE_DIRECTORY:
		return("DIR");
	case HAMMER2_OBJTYPE_REGFILE:
		return("FILE");
	case HAMMER2_OBJTYPE_FIFO:
		return("FIFO");
	case HAMMER2_OBJTYPE_CDEV:
		return("CDEV");
	case HAMMER2_OBJTYPE_BDEV:
		return("BDEV");
	case HAMMER2_OBJTYPE_SOFTLINK:
		return("SOFTLINK");
	case HAMMER2_OBJTYPE_SOCKET:
		return("SOCKET");
	case HAMMER2_OBJTYPE_WHITEOUT:
		return("WHITEOUT");
	default:
		return("ILLEGAL");
	}
}

const char *
hammer2_pfstype_to_str(uint8_t type)
{
	switch(type) {
	case HAMMER2_PFSTYPE_NONE:
		return("NONE");
	case HAMMER2_PFSTYPE_SUPROOT:
		return("SUPROOT");
	case HAMMER2_PFSTYPE_MASTER:
		return("MASTER");
	default:
		return("ILLEGAL");
	}
}

const char *
hammer2_pfssubtype_to_str(uint8_t subtype)
{
	switch(subtype) {
	case HAMMER2_PFSSUBTYPE_NONE:
		return("NONE");
	default:
		return("ILLEGAL");
	}
}

const char *
hammer2_breftype_to_str(uint8_t type)
{
	switch(type) {
	case HAMMER2_BREF_TYPE_EMPTY:
		return("empty");
	case HAMMER2_BREF_TYPE_INODE:
		return("inode");
	case HAMMER2_BREF_TYPE_INDIRECT:
		return("indirect");
	case HAMMER2_BREF_TYPE_DATA:
		return("data");
	case HAMMER2_BREF_TYPE_DIRENT:
		return("dirent");
	case HAMMER2_BREF_TYPE_FREEMAP_NODE:
		return("freemap_node");
	case HAMMER2_BREF_TYPE_FREEMAP_LEAF:
		return("freemap_leaf");
	case HAMMER2_BREF_TYPE_INVALID:
		return("invalid");
	case HAMMER2_BREF_TYPE_FREEMAP:
		return("freemap");
	case HAMMER2_BREF_TYPE_VOLUME:
		return("volume");
	default:
		return("unknown");
	}
}

const char *
sizetostr(hammer2_off_t size)
{
	static char buf[32];

	if (size < 1024 / 2) {
		snprintf(buf, sizeof(buf), "%6.2fB", (double)size);
	} else if (size < 1024 * 1024 / 2) {
		snprintf(buf, sizeof(buf), "%6.2fKB",
			(double)size / 1024);
	} else if (size < 1024 * 1024 * 1024LL / 2) {
		snprintf(buf, sizeof(buf), "%6.2fMB",
			(double)size / (1024 * 1024));
	} else if (size < 1024 * 1024 * 1024LL * 1024LL / 2) {
		snprintf(buf, sizeof(buf), "%6.2fGB",
			(double)size / (1024 * 1024 * 1024LL));
	} else {
		snprintf(buf, sizeof(buf), "%6.2fTB",
			(double)size / (1024 * 1024 * 1024LL * 1024LL));
	}
	return(buf);
}

const char *
counttostr(hammer2_off_t size)
{
	static char buf[32];

	if (size < 1024 / 2) {
		snprintf(buf, sizeof(buf), "%jd",
			 (intmax_t)size);
	} else if (size < 1024 * 1024 / 2) {
		snprintf(buf, sizeof(buf), "%jd",
			 (intmax_t)size);
	} else if (size < 1024 * 1024 * 1024LL / 2) {
		snprintf(buf, sizeof(buf), "%6.2fM",
			 (double)size / (1024 * 1024));
	} else if (size < 1024 * 1024 * 1024LL * 1024LL / 2) {
		snprintf(buf, sizeof(buf), "%6.2fG",
			 (double)(size / (1024 * 1024 * 1024LL)));
	} else {
		snprintf(buf, sizeof(buf), "%6.2fT",
			 (double)(size / (1024 * 1024 * 1024LL * 1024LL)));
	}
	return(buf);
}

hammer2_off_t
check_volume(int fd)
{
	struct stat st;
	hammer2_off_t size;

	/*
	 * Get basic information about the volume
	 */
	if (ioctl(fd, DIOCGMEDIASIZE, &size) < 0) {
		/*
		 * Allow the formatting of regular files as HAMMER2 volumes
		 */
		if (fstat(fd, &st) < 0)
			err(1, "Unable to stat fd %d", fd);
		if (!S_ISREG(st.st_mode))
			errx(1, "Unsupported file type for fd %d", fd);
		size = st.st_size;
	} else {
		/*
		 * When formatting a block device as a HAMMER2 volume the
		 * sector size must be compatible.  HAMMER2 uses 64K
		 * filesystem buffers but logical buffers for direct I/O
		 * can be as small as HAMMER2_LOGSIZE (16KB).
		 */
		int media_blksize;
		if (!ioctl(fd, DIOCGSECTORSIZE, &media_blksize) &&
		    (media_blksize > HAMMER2_PBUFSIZE ||
		    HAMMER2_PBUFSIZE % media_blksize)) {
			errx(1, "A media sector size of %d is not supported",
			     media_blksize);
		}
	}
	return(size);
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
dirhash(const unsigned char *name, size_t len)
{
	const unsigned char *aname = name;
	uint32_t crcx;
	uint64_t key;
	size_t i, j;

	key = 0;

	/*
	 * m32
	 */
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

char **
get_hammer2_mounts(int *acp)
{
	struct statfs *fs;
	char **av;
	int n;
	int w;
	int i;

	/*
	 * Get a stable list of mount points
	 */
again:
	n = getfsstat(NULL, 0, MNT_NOWAIT);
	av = calloc(n, sizeof(char *));
	fs = calloc(n, sizeof(struct statfs));
	if (getfsstat(fs, sizeof(*fs) * n, MNT_NOWAIT) != n) {
		free(av);
		free(fs);
		goto again;
	}

	/*
	 * Pull out hammer2 filesystems only
	 */
	for (i = w = 0; i < n; ++i) {
		if (strcmp(fs[i].f_fstypename, "hammer2") != 0)
			continue;
		av[w++] = strdup(fs[i].f_mntonname);
	}
	*acp = w;
	free(fs);

	return av;
}

void
put_hammer2_mounts(int ac, char **av)
{
	while (--ac >= 0)
		free(av[ac]);
	free(av);
}
