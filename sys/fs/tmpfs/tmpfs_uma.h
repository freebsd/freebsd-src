/*-
 * Copyright (c) 2007 Rohit Jalan (rohitj@purpe.com)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _TMPFS_UMA_H
#define _TMPFS_UMA_H

#include <vm/uma.h>

#define	TMPFS_STRZONE_ZONECOUNT		7
#define TMPFS_STRZONE_STARTLEN		(1 << 4)

struct tmpfs_mount;

struct tmpfs_str_zone {
	uma_zone_t		tsz_zone[TMPFS_STRZONE_ZONECOUNT];
};

void	tmpfs_str_zone_create(struct tmpfs_str_zone *);
void	tmpfs_str_zone_destroy(struct tmpfs_str_zone *);

static __inline char*
tmpfs_str_zone_alloc(struct tmpfs_str_zone *tsz, int flags, size_t len)
{

	size_t i, zlen;
	char *ptr;

	MPASS(len <= (TMPFS_STRZONE_STARTLEN << (TMPFS_STRZONE_ZONECOUNT-1)));

	i = 0;
	zlen = TMPFS_STRZONE_STARTLEN;
	while (len > zlen) {
		++i;
		zlen  <<= 1;
	}
	ptr = (char *)uma_zalloc(tsz->tsz_zone[i], flags);
	return  ptr;
}

static __inline void
tmpfs_str_zone_free(struct tmpfs_str_zone *tsz, char *item, size_t len)
{
	size_t i, zlen;

	MPASS(len <= (TMPFS_STRZONE_STARTLEN << (TMPFS_STRZONE_ZONECOUNT-1)));

	i = 0;
	zlen = TMPFS_STRZONE_STARTLEN;
	while (len > zlen) {
		++i;
		zlen  <<= 1;
	}
	uma_zfree(tsz->tsz_zone[i], item);
}

#endif
