/*-
 * Copyright (c) 2014-2016 Robert N. M. Watson
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

#if !__has_feature(capabilities)
#error "This code requires a CHERI-aware compiler"
#endif

#include <sys/types.h>
#include <sys/param.h>

#include <cheri/cheri.h>
#include <cheri/cheric.h>

#include <machine/sysarch.h>

#include <assert.h>

#include "cheri_type.h"

static __capability void *cheri_type_root;
static uint64_t cheri_type_next = 1;

static void
cheri_type_init(void)
{

	/*
	 * Request a root type capability from the kernel.  Ensure it is set
	 * to NULL if this fails, as failure could represent partial copyout()
	 * with confusing failure modes, etc.  We can validate properties of
	 * the capability later, should compartmentalisation actually be used
	 * by the application.
	 */
	if (sysarch(CHERI_GET_TYPECAP, &cheri_type_root) < 0)
		cheri_type_root = NULL;
	assert((cheri_getperm(cheri_type_root) & CHERI_PERM_SEAL) != 0);
	assert(cheri_getlen(cheri_type_root) != 0);
}

/*
 * A [very] simple CHERI type allocator.
 *
 * XXXRW: Concurrency.  We need locks around this.
 */
__capability void *
cheri_type_alloc(void)
{
	__capability void *new_type_cap;

	assert(cheri_type_next < 1<<24);

	/*
	 * On first use, query the root object-type capability from the
	 * kernel.
	 */
	if ((cheri_getperm(cheri_type_root) & CHERI_PERM_SEAL) == 0)
		cheri_type_init();
	new_type_cap = cheri_maketype(cheri_type_root, cheri_type_next);
	cheri_type_next++;
	return (new_type_cap);
}
