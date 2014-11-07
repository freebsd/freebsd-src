/*-
 * Copyright (c) 2014 Robert N. M. Watson
 * Copyright (c) 2014 SRI International
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

#include <machine/cheri.h>
#include <machine/cheric.h>

#include <cheri/cheri_invoke.h>
#include <cheri/cheri_system.h>

#include <stdlib.h>

/*
 * This C file contains stubs to invoke the CHERI system class, one per
 * method.
 */
static struct cheri_object cheri_system_object;

void
cheri_system_setup(struct cheri_object system_object)
{

	cheri_system_object = system_object;
}

register_t cheri_system_methodnum_helloworld = CHERI_SYSTEM_METHOD_HELLOWORLD;
int
cheri_system_helloworld(void)
{

	return (cheri_invoke(cheri_system_object,
	    cheri_system_methodnum_helloworld, 0, 0, 0, 0, 0, 0, 0,
	    cheri_zerocap(), cheri_zerocap(), cheri_zerocap(),
	    cheri_zerocap(), cheri_zerocap(), cheri_zerocap(),
	    cheri_zerocap(), cheri_zerocap()));
}

register_t cheri_system_methodnum_puts = CHERI_SYSTEM_METHOD_PUTS;
int
cheri_system_puts(__capability const char *str)
{
	__capability char *str_noconst;

	str_noconst = (__capability char *)str;
	return (cheri_invoke(cheri_system_object,
	    cheri_system_methodnum_puts, 0, 0, 0, 0, 0, 0, 0, str_noconst,
	    cheri_zerocap(), cheri_zerocap(), cheri_zerocap(),
	    cheri_zerocap(), cheri_zerocap(), cheri_zerocap(),
	    cheri_zerocap()));
}

register_t cheri_system_methodnum_putchar = CHERI_SYSTEM_METHOD_PUTCHAR;
int
cheri_system_putchar(int c)
{

	return (cheri_invoke(cheri_system_object,
	    cheri_system_methodnum_putchar, c, 0, 0, 0, 0, 0, 0,
	    cheri_zerocap(), cheri_zerocap(), cheri_zerocap(),
	    cheri_zerocap(), cheri_zerocap(), cheri_zerocap(),
	    cheri_zerocap(), cheri_zerocap()));
}

register_t cheri_system_methodnum_clock_gettime = CHERI_SYSTEM_CLOCK_GETTIME;
int
cheri_system_clock_gettime(clockid_t clock_id, __capability struct timespec *tp)
{

	return (cheri_invoke(cheri_system_object,
	    cheri_system_methodnum_clock_gettime, clock_id, 0, 0, 0, 0, 0, 0,
	    tp,
	    cheri_zerocap(), cheri_zerocap(), cheri_zerocap(),
	    cheri_zerocap(), cheri_zerocap(), cheri_zerocap(),
	    cheri_zerocap()));
}

register_t cheri_system_methodnum_calloc = CHERI_SYSTEM_CALLOC;
int
cheri_system_calloc(size_t number, size_t size, void **ptrp)
{

	return (cheri_invoke(cheri_system_object,
	    cheri_system_methodnum_calloc, number, size, 0, 0, 0, 0, 0, ptrp,
	    cheri_zerocap(), cheri_zerocap(), cheri_zerocap(),
	    cheri_zerocap(), cheri_zerocap(), cheri_zerocap(),
	    cheri_zerocap()));
}

register_t cheri_system_methodnum_free = CHERI_SYSTEM_FREE;
int
cheri_system_free(const void *ptr)
{

	cheri_invoke(cheri_system_object,
	    cheri_system_methodnum_free, 0, 0, 0, 0, 0, 0, 0, ptr,
	    cheri_zerocap(), cheri_zerocap(), cheri_zerocap(),
	    cheri_zerocap(), cheri_zerocap(), cheri_zerocap(),
	    cheri_zerocap());
}
