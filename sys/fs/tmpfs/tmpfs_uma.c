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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <vm/vm.h>

#include <fs/tmpfs/tmpfs.h>

uma_zone_t
tmpfs_zone_create(char *name, int size, int align, struct tmpfs_mount *tmp)
{
	uma_zone_t z;
	z = uma_zcreate(name, size, NULL, NULL, NULL, NULL, align, M_WAITOK);
	return z;
}

void
tmpfs_zone_destroy(uma_zone_t zone)
{
	uma_zdestroy(zone);
}

void
tmpfs_str_zone_create(struct tmpfs_str_zone *tsz, struct tmpfs_mount *tmp)
{
	int i, len;

	len = TMPFS_STRZONE_STARTLEN;
	for (i = 0; i < TMPFS_STRZONE_ZONECOUNT; ++i) {
		tsz->tsz_zone[i] = tmpfs_zone_create(
					"TMPFS str", len, UMA_ALIGN_PTR, tmp);
		len <<= 1;
	}
}

void
tmpfs_str_zone_destroy(struct tmpfs_str_zone *tsz)
{
	int i, len;

	len = TMPFS_STRZONE_STARTLEN;
	for (i = 0; i < TMPFS_STRZONE_ZONECOUNT; ++i) {
		tmpfs_zone_destroy(tsz->tsz_zone[i]);
		len <<= 1;
	}
}

