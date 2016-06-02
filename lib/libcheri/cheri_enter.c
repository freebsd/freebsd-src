/*-
 * Copyright (c) 2014, 2016 Robert N. M. Watson
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
#include <sys/mman.h>

#include <cheri/cheri.h>
#include <cheri/cheric.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "cheri_enter.h"
#include "cheri_system.h"
#include "sandbox.h"

/*
 * This file implements a stack landing pad for CHERI system classes provided
 * by libcheri.  The single stack is statically allocated -- meaning no
 * concurrent invocation from sandboxes.  Currently, that is ensured by virtue
 * of applications not themselves invoking sandboxes concurrently.
 */

/*
 * Stack for use on entering from sandbox.
 */
#define	CHERI_ENTER_STACK_SIZE	(PAGE_SIZE * 16)
static void		*__cheri_enter_stack;
__capability void	*__cheri_enter_stack_cap;
register_t		 __cheri_enter_stack_sp;

__attribute__ ((constructor)) static void
cheri_enter_init(void)
{

	/* XXX: Should be MAP_STACK, but that is broken. */
	__cheri_enter_stack = mmap(NULL, CHERI_ENTER_STACK_SIZE,
	    PROT_READ | PROT_WRITE, MAP_ANON, -1, 0);
	assert(__cheri_enter_stack != MAP_FAILED);

	/*
	 * In CheriABI, we use the capability returned by mmap(2), which $sp
	 * will be relative to.  Otherwise, use $c0 and assume a global $sp.
	 */
#ifdef __CHERI_PURE_CAPABILITY__
	__cheri_enter_stack_cap = __cheri_enter_stack;
	__cheri_enter_stack_sp = CHERI_ENTER_STACK_SIZE;
#else
	__cheri_enter_stack_cap = cheri_getdefault();
	__cheri_enter_stack_sp = (register_t)((char *)__cheri_enter_stack +
	    CHERI_ENTER_STACK_SIZE);
#endif
}
