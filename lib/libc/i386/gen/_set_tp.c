/*-
 * Copyright (c) 2004 Doug Rabson
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
 *
 *	$FreeBSD$
 */

#include <string.h>
#include <stdint.h>
#include <machine/segments.h>
#include <machine/sysarch.h>

void
_set_tp(void *tp)
{
#ifndef COMPAT_32BIT
	union descriptor ldt;
	int error, sel;

	error = i386_set_gsbase(tp);
	if (error == 0)
		return;
	memset(&ldt, 0, sizeof(ldt));
	ldt.sd.sd_lolimit = 0xffff;	/* 4G limit */
	ldt.sd.sd_lobase = ((uintptr_t)tp) & 0xffffff;
	ldt.sd.sd_type = SDT_MEMRWA;
	ldt.sd.sd_dpl = SEL_UPL;
	ldt.sd.sd_p = 1;		/* present */
	ldt.sd.sd_hilimit = 0xf;	/* 4G limit */
	ldt.sd.sd_def32 = 1;		/* 32 bit */
	ldt.sd.sd_gran = 1;		/* limit in pages */
	ldt.sd.sd_hibase = (((uintptr_t)tp) >> 24) & 0xff;
	sel = i386_set_ldt(LDT_AUTO_ALLOC, &ldt, 1);
	__asm __volatile("movl %0,%%gs" : : "rm" ((sel << 3) | 7));
#else
	i386_set_gsbase(tp);
#endif
}
