/*-
 * Copyright (c) 2014-2017 Robert N. M. Watson
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
 * ("CTSRD"), as part of the DARPA CRASH research programme.
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

__REQUIRE_CAPABILITIES

#include <sys/types.h>
#include <sys/param.h>

#include <cheri/cheri.h>
#include <cheri/cheric.h>

#include <machine/sysarch.h>

#include <assert.h>
#include <stdatomic.h>
#include <stdio.h>

#include "cheri_type.h"

/* The root sealing capability of the types provenance tree. */
static __capability void *cheri_sealing_root;

/* The number of bits in the type field of a capability. */
static const int cap_type_bits = 24;

/* The next non-system type number to allocate. */
static _Atomic(uint64_t) cheri_type_next = 1;

/* The maximum non-system type number to allocate (plus one). */
static const int cheri_type_max = 1<<(cap_type_bits-2);

/* The next system type number to allocate. */
static _Atomic(uint64_t) cheri_system_type_next = cheri_type_max;

/* The maximum system type number to allocate (plus one). */
static const int cheri_system_type_max = 1<<(cap_type_bits-1);

static void
cheri_type_init(void)
{

	/*
	 * Request a root sealing capability from the kernel.  Ensure it is
	 * set to NULL if this fails, as failure could represent partial
	 * copyout() with confusing failure modes, etc.  We can validate
	 * properties of the capability later, should compartmentalisation
	 * actually be used by the application.
	 */
	if (sysarch(CHERI_GET_SEALCAP, &cheri_sealing_root) < 0)
		cheri_sealing_root = NULL;
	assert((cheri_getperm(cheri_sealing_root) & CHERI_PERM_SEAL) != 0);
	assert(cheri_getlen(cheri_sealing_root) != 0);
}

/**
 * A simple type allocator.  The `source` argument specifies the pointer value
 * that will be atomically incremented and whose current value should give the
 * integer representation of the type to allocate.  The `max` argument
 * specifies value that the returned type must be less than.
 */
static inline __capability void *
alloc_type_capability(_Atomic(uint64_t) *source, uint64_t max)
{
	__capability void *new_type_cap;
	uint64_t next;

	/*
	 * We require that this counter be strictly monotonic, but we don't
	 * need to establish a happens-before relationship with any other
	 * thread.
	 */
	next = atomic_fetch_add_explicit(source, 1, memory_order_relaxed);

	/*
	 * If we've run out of types, return NULL so that we get an obvious
	 * failure.
	 */
	if (next > max) {
		return (NULL);
	}

	/*
	 * On first use, query the root object-type capability from the
	 * kernel.
	 */
	if ((cheri_getperm(cheri_sealing_root) & CHERI_PERM_SEAL) == 0)
		cheri_type_init();
	new_type_cap = cheri_maketype(cheri_sealing_root, next);
	return (new_type_cap);
}

/*
 * A [very] simple CHERI type allocator.
 */
__capability void *
cheri_type_alloc(void)
{

	return (alloc_type_capability(&cheri_type_next, cheri_type_max));
}

__capability void *
cheri_system_type_alloc(void)
{

	return (alloc_type_capability(&cheri_system_type_next,
	    cheri_system_type_max));
}
