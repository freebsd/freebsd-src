/*-
 * Copyright (c) 1999 Luoqi Chen <luoqi@freebsd.org>
 * All rights reserved.
 * Copyright (c) 2000 Jake Burkholder <jake@freebsd.org>
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
 * $FreeBSD$
 */

#ifndef	_MACHINE_GLOBALS_H_
#define	_MACHINE_GLOBALS_H_

#ifdef _KERNEL

#include <machine/globaldata.h>

static __inline struct globaldata *
_global_globaldata(void)
{
	struct globaldata *gd;
	int offset;

	offset = offsetof(struct globaldata, gd_prvspace);
	__asm __volatile("movl %%fs:%1,%0"
	    : "=r" (gd)
	    : "m" (*(struct globaldata *)(offset)));

	return (gd);
}

#define	GLOBALDATA		(_global_globaldata())

#define	PCPU_GET(member)	(GLOBALDATA->gd_ ## member)
#define	PCPU_PTR(member)	(&GLOBALDATA->gd_ ## member)
#define	PCPU_SET(member, val)	(GLOBALDATA->gd_ ## member = (val))

#define	CURPROC			PCPU_GET(curproc)
#define	CURTHD			PCPU_GET(curproc)

#define	astpending		PCPU_GET(astpending)
#define	common_tss		(*PCPU_PTR(common_tss))
#define	common_tssd		(*PCPU_PTR(common_tssd))
#define	cpuid			PCPU_GET(cpuid)
#define	currentldt		PCPU_GET(currentldt)
#define	curpcb			PCPU_GET(curpcb)
#define	curproc			PCPU_GET(curproc)
#define	idleproc		PCPU_GET(idleproc)
#define	inside_intr		(*PCPU_PTR(inside_intr))
#define	intr_nesting_level	PCPU_GET(intr_nesting_level)
#define	npxproc			PCPU_GET(npxproc)
#define	prv_CMAP1		(*PCPU_PTR(prv_CMAP1))
#define	prv_CMAP2		(*PCPU_PTR(prv_CMAP2))
#define	prv_CMAP3		(*PCPU_PTR(prv_CMAP3))
#define	prv_PMAP1		(*PCPU_PTR(prv_PMAP1))
#define	prv_CADDR1		PCPU_GET(prv_CADDR1)
#define	prv_CADDR2		PCPU_GET(prv_CADDR2)
#define	prv_CADDR3		PCPU_GET(prv_CADDR3)
#define	prv_PADDR1		PCPU_GET(prv_PADDR1)
#define	other_cpus		(*PCPU_PTR(other_cpus))
#define	switchticks		(*PCPU_PTR(switchticks))
#define	switchtime		(*PCPU_PTR(switchtime))
#define	tss_gdt			(*PCPU_PTR(tss_gdt))
#define	witness_spin_check	PCPU_GET(witness_spin_check)

#endif	/* _KERNEL */

#endif	/* !_MACHINE_GLOBALS_H_ */
