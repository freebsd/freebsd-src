/*-
 * Copyright (c) 2014 Robert N. M. Watson
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

#include <machine/cheri.h>
#include <machine/cheric.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "cheri_enter.h"
#include "cheri_system.h"
#include "sandbox.h"

/*
 * This file implements the "landing pad" for CHERI method invocations on
 * system capabilities.  For now, several pretty critical limitations, which
 * we will work on relaxing over time:
 *
 * 1. one global invocation stack, so no concurrency.
 * 2. one global data object, so no support for multiple data capabilities.
 */
register_t	cheri_enter(register_t methodnum, register_t a1,
		    register_t a2, register_t a3, register_t a4,
		    register_t a5, register_t a6, register_t a7,
		    struct cheri_object system_object,
		    __capability void *c3, __capability void *c4,
		    __capability void *c5, __capability void *c6,
		    __capability void *c7) __attribute__((cheri_ccall));

/*
 * Stack for use on entering from sandbox.
 */
#define	CHERI_ENTER_STACK_SIZE	(PAGE_SIZE * 4)
static void *__cheri_enter_stack;
void *__cheri_enter_stack_top;

static cheri_enter_fn_t	cheri_user_fn_ptr;

__capability struct sandbox *
cheri_enter_getsandbox(void)
{

	return (cheri_getidc());
}

__attribute__ ((constructor)) static void
cheri_enter_init(void)
{

	__cheri_enter_stack = mmap(NULL, CHERI_ENTER_STACK_SIZE,
	    PROT_READ | PROT_WRITE, MAP_STACK, -1, 0);
	assert(__cheri_enter_stack != MAP_FAILED);
	__cheri_enter_stack_top = (char *)__cheri_enter_stack +
	    CHERI_ENTER_STACK_SIZE;
}

/*
 * Allow the user application to register its own methods.
 */
void
cheri_enter_register_fn(cheri_enter_fn_t fn_ptr)
{

	cheri_user_fn_ptr = fn_ptr;
}

/*
 * cheri_enter() itself: sandbox invocations turn up here.
 */
register_t
cheri_enter(register_t methodnum, register_t a1, register_t a2, register_t a3,
    register_t a4, register_t a5, register_t a6, register_t a7,
    struct cheri_object system_object __unused, __capability void *c3,
    __capability void *c4, __capability void *c5, __capability void *c6,
    __capability void *c7)
{

	switch (methodnum) {
	case CHERI_SYSTEM_METHOD_HELLOWORLD:
		return (cheri_system_helloworld());

	case CHERI_SYSTEM_METHOD_PUTS:
		return (cheri_system_puts(c3));

	case CHERI_SYSTEM_METHOD_PUTCHAR:
		return (cheri_system_putchar(a1));

	default:
		if (methodnum >= CHERI_ENTER_USER_BASE &&
		    methodnum < CHERI_ENTER_USER_CEILING &&
		    cheri_user_fn_ptr != NULL)
			return (cheri_user_fn_ptr(methodnum, a1, a2, a3,
			    a4, a5, a6, a7, system_object, c3, c4, c5, c6,
			    c7));
		return (-1);
	}
}
