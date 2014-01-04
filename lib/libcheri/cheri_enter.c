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

#include <sys/types.h>
#include <sys/param.h>
#include <sys/mman.h>

#include <machine/cheri.h>
#include <machine/cheric.h>

#include <assert.h>
#include <stdlib.h>

/*
 * This file implements the "landing pad" for CHERI method invocations on
 * system capabilities.  For now, several pretty critical limitations, which
 * we will work on relaxing over time:
 *
 * 1. one global invocation stack, so no concurrency.
 * 2. one global data object, so no support for multiple data capabilities.
 */
register_t	cheri_enter(register_t methodnum, register_t a0,
		    register_t a1, register_t a2, register_t a3,
		    register_t a4, register_t a5, register_t a6,
		    register_t a7);

/*
 * Stack for use on entering from sandbox.
 */
#define	CHERI_ENTER_STACK_SIZE	(PAGE_SIZE * 4)
void *__cheri_enter_stack;

__attribute__ ((constructor)) static void
cheri_enter_init(void)
{

	__cheri_enter_stack = mmap(NULL, CHERI_ENTER_STACK_SIZE,
	    PROT_READ | PROT_WRITE, MAP_STACK, -1, 0);
	assert(__cheri_enter_stack != MAP_FAILED);
}

register_t
cheri_enter(register_t methodnum __unused, register_t a0 __unused,
    register_t a1 __unused, register_t a2 __unused, register_t a3 __unused,
    register_t a4 __unused, register_t a5 __unused, register_t a6 __unused,
    register_t a7 __unused)
{

	return (123456);
}
