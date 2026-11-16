/*-
 * Copyright (c) 2026 François Degros
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "test.h"

#define __LIBARCHIVE_BUILD 1
#include "archive_time_private.h"

/* NTFS epoch offset: seconds between 1601-01-01 and 1970-01-01. */
#define NTFS_EPOCH_SECS 11644473600LL
#define NTFS_TICKS_PER_SEC 10000000LL

DEFINE_TEST(test_archive_ntfs_to_unix)
{
	int64_t secs;
	uint32_t nsecs;

	/* Exactly the Unix epoch. */
	__archive_ntfs_to_unix(
	    (uint64_t)(NTFS_EPOCH_SECS * NTFS_TICKS_PER_SEC), &secs, &nsecs);
	assertEqualInt(0, secs);
	assertEqualInt(0, nsecs);

	/* A normal post-1970, second-aligned date. */
	__archive_ntfs_to_unix(
	    (uint64_t)((NTFS_EPOCH_SECS + 1000000000LL) * NTFS_TICKS_PER_SEC),
	    &secs, &nsecs);
	assertEqualInt(1000000000, secs);
	assertEqualInt(0, nsecs);

	/* A normal post-1970 date with sub-second precision. */
	__archive_ntfs_to_unix(
	    (uint64_t)((NTFS_EPOCH_SECS + 1000000000LL) * NTFS_TICKS_PER_SEC
		+ 1234567), &secs, &nsecs);
	assertEqualInt(1000000000, secs);
	assertEqualInt(123456700, nsecs);

	/* A pre-1970, second-aligned date: FILETIME has no sign ambiguity
	 * (it's unsigned ticks since 1601), so this is perfectly legitimate,
	 * not malformed. */
	__archive_ntfs_to_unix(
	    (uint64_t)(NTFS_EPOCH_SECS * NTFS_TICKS_PER_SEC
		- 2 * NTFS_TICKS_PER_SEC), &secs, &nsecs);
	assertEqualInt(-2, secs);
	assertEqualInt(0, nsecs);

	/* A pre-1970, non-second-aligned date. lldiv() truncates toward
	 * zero, not toward -infinity, so this specifically exercises the
	 * floor-division adjustment: nsecs must land in
	 * [0, 999999999], with the sign folded entirely into secs, not a
	 * huge value from a negative remainder miscast to uint32_t. */
	__archive_ntfs_to_unix(
	    (uint64_t)(NTFS_EPOCH_SECS * NTFS_TICKS_PER_SEC - 15),
	    &secs, &nsecs);
	assertEqualInt(-1, secs);
	assertEqualInt(999998500, nsecs);

	/* Same, one full second further back, to double check the whole-
	 * second component also keeps adjusting correctly alongside the
	 * fractional one. */
	__archive_ntfs_to_unix(
	    (uint64_t)(NTFS_EPOCH_SECS * NTFS_TICKS_PER_SEC
		- NTFS_TICKS_PER_SEC - 15), &secs, &nsecs);
	assertEqualInt(-2, secs);
	assertEqualInt(999998500, nsecs);
}
