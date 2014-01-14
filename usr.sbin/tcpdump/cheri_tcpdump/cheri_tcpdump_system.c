/*-
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

#include <machine/cheri.h>

#include <sys/types.h>

#include <cheri/sandbox.h>
#include <cheri/cheri_enter.h>
#include <cheri/cheri_system.h>

#include <stdio.h>
#include <vis.h>

#include "cheri_tcpdump_system.h"

static int
cheri_tcpdump_puts(__capability const char *str)
{
	char tmpbuf[5];
	char *bp;

	for (; *str != '\0'; str++) {
		vis(tmpbuf, *str, VIS_CSTYLE, 0);
		for (bp = tmpbuf; bp != '\0'; bp++)
			putchar(*bp);
	}
	return (0);
}

static int
cheri_tcpdump_putchar(int c)
{
#if 0
	char tmpbuf[5];
	char *bp;

	vis(tmpbuf, c, VIS_CSTYLE, 0);
	for (bp = tmpbuf; bp != '\0'; bp++)
		putchar(*bp);
	return (0);
#endif
	return (putchar(c));
}

register_t
cheri_tcpdump_enter(register_t methodnum, register_t a1,
	    register_t a2, register_t a3, register_t a4, register_t a5,
	    register_t a6, register_t a7, struct cheri_object system_object,
	    __capability void *c3, __capability void *c4,
	    __capability void *c5, __capability void *c6,
	    __capability void *c7)
{
	const char red_bg[] = "\x1b[41m";

	switch (methodnum) {
	case CHERI_TCPDUMP_PUTS:
		return (cheri_tcpdump_puts(c3));

	case CHERI_TCPDUMP_PUTS_PAWNED:
		printf(red_bg);
		return (cheri_tcpdump_puts(c3));

	case CHERI_TCPDUMP_PUTCHAR:
		return (cheri_tcpdump_putchar(a1));

	case CHERI_TCPDUMP_PUTCHAR_PAWNED:
		printf(red_bg);
		return (cheri_tcpdump_putchar(a1));

	default:
		return (-1);
	}
}
