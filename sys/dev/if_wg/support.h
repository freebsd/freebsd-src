/* SPDX-License-Identifier: MIT
 *
 * Copyright (C) 2021 Jason A. Donenfeld <Jason@zx2c4.com>. All Rights Reserved.
 * Copyright (C) 2021 Matt Dunwoodie <ncon@noconroy.net>
 */

#ifndef _WG_SUPPORT
#define _WG_SUPPORT

#include <sys/types.h>
#include <sys/limits.h>
#include <sys/endian.h>
#include <sys/libkern.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/lock.h>
#include <vm/uma.h>

/* TODO the following is openbsd compat defines to allow us to copy the wg_*
 * files from openbsd (almost) verbatim. this will greatly increase maintenance
 * across the platforms. it should be moved to it's own file. the only thing
 * we're missing from this is struct pool (freebsd: uma_zone_t), which isn't a
 * show stopper, but is something worth considering in the future.
 *  - md */

#define rw_assert_wrlock(x) rw_assert(x, RA_WLOCKED)
#define rw_enter_write rw_wlock
#define rw_exit_write rw_wunlock
#define rw_enter_read rw_rlock
#define rw_exit_read rw_runlock
#define rw_exit rw_unlock

#define RW_DOWNGRADE 1
#define rw_enter(x, y) do {		\
	CTASSERT(y == RW_DOWNGRADE);	\
	rw_downgrade(x);		\
} while (0)

MALLOC_DECLARE(M_WG);

#include <crypto/siphash/siphash.h>
typedef struct {
	uint64_t	k0;
	uint64_t	k1;
} SIPHASH_KEY;

static inline uint64_t
siphash24(const SIPHASH_KEY *key, const void *src, size_t len)
{
	SIPHASH_CTX ctx;

	return (SipHashX(&ctx, 2, 4, (const uint8_t *)key, src, len));
}
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

#endif
