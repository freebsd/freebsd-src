/*-
 * Copyright (c) 2000-2013 Mark R. V. Murray
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
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

#ifndef	_SYS_RANDOM_H_
#define	_SYS_RANDOM_H_

#ifdef _KERNEL

#include <sys/types.h>

#include "opt_random.h"

#if defined(RANDOM_LOADABLE) && defined(RANDOM_YARROW)
#error "Cannot define both RANDOM_LOADABLE and RANDOM_YARROW"
#endif

struct uio;

#if defined(DEV_RANDOM)
u_int read_random(void *, u_int);
int read_random_uio(struct uio *, bool);
#else
static __inline int
read_random_uio(void *a __unused, u_int b __unused)
{
	return (0);
}
static __inline u_int
read_random(void *a __unused, u_int b __unused)
{
	return (0);
}
#endif

/*
 * Note: if you add or remove members of random_entropy_source, remember to also update the
 * KASSERT regarding what valid members are in random_harvest_internal(), and remember the
 * strings in the static array random_source_descr[] in random_harvestq.c.
 *
 * NOTE: complain loudly to markm@ or on the lists if this enum gets more than 32
 * distinct values (0-31)! ENTROPYSOURCE may be == 32, but not > 32.
 */
enum random_entropy_source {
	RANDOM_START = 0,
	RANDOM_CACHED = 0,
	/* Environmental sources */
	RANDOM_ATTACH,
	RANDOM_KEYBOARD,
	RANDOM_MOUSE,
	RANDOM_NET_TUN,
	RANDOM_NET_ETHER,
	RANDOM_NET_NG,
	RANDOM_INTERRUPT,
	RANDOM_SWI,
	RANDOM_FS_ATIME,
	RANDOM_FAST,	/* Special!! Miscellaneous high performance stuff, like UMA/SLAB Allocator */
	RANDOM_ENVIRONMENTAL_END = RANDOM_FAST,
	/* Fast hardware random-number sources from here on. */
	RANDOM_PURE_OCTEON,
	RANDOM_PURE_SAFE,
	RANDOM_PURE_GLXSB,
	RANDOM_PURE_UBSEC,
	RANDOM_PURE_HIFN,
	RANDOM_PURE_RDRAND,
	RANDOM_PURE_NEHEMIAH,
	RANDOM_PURE_RNDTEST,
	RANDOM_PURE_VIRTIO,
	ENTROPYSOURCE
};

#define RANDOM_HARVEST_EVERYTHING_MASK ((1 << (RANDOM_ENVIRONMENTAL_END + 1)) - 1)

#if defined(DEV_RANDOM)
void random_harvest_queue(const void *, u_int, u_int, enum random_entropy_source);
void random_harvest_fast(const void *, u_int, u_int, enum random_entropy_source);
void random_harvest_direct(const void *, u_int, u_int, enum random_entropy_source);
#else
#define random_harvest_queue(a, b, c, d) do {} while (0)
#define random_harvest_fast(a, b, c, d) do {} while (0)
#define random_harvest_direct(a, b, c, d) do {} while (0)
#endif

#endif /* _KERNEL */

#endif /* _SYS_RANDOM_H_ */
