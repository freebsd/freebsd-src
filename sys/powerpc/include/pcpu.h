/*-
 * Copyright (c) 1999 Luoqi Chen <luoqi@freebsd.org>
 * Copyright (c) Peter Wemm <peter@netplex.com.au>
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
 * $FreeBSD: src/sys/powerpc/include/pcpu.h,v 1.24 2007/06/04 21:38:47 attilio Exp $
 */

#ifndef	_MACHINE_PCPU_H_
#define	_MACHINE_PCPU_H_

#ifdef _KERNEL
#include <machine/cpufunc.h>

struct pmap;
#define	CPUSAVE_LEN	8

#define	PCPU_MD_FIELDS							\
	int		pc_inside_intr;					\
	struct pmap	*pc_curpmap;		/* current pmap */	\
        struct thread   *pc_fputhread;          /* current fpu user */  \
	register_t	pc_tempsave[CPUSAVE_LEN];			\
	register_t	pc_disisave[CPUSAVE_LEN];			\
	register_t	pc_dbsave[CPUSAVE_LEN];

/* Definitions for register offsets within the exception tmp save areas */
#define	CPUSAVE_R28	0		/* where r28 gets saved */
#define	CPUSAVE_R29	1		/* where r29 gets saved */
#define	CPUSAVE_R30	2		/* where r30 gets saved */
#define	CPUSAVE_R31	3		/* where r31 gets saved */
#define	CPUSAVE_DAR	4		/* where SPR_DAR gets saved */
#define	CPUSAVE_DSISR	5		/* where SPR_DSISR gets saved */
#define	CPUSAVE_SRR0	6		/* where SRR0 gets saved */
#define	CPUSAVE_SRR1	7		/* where SRR1 gets saved */

#define PCPUP	((struct pcpu *) powerpc_get_pcpup())

#define	PCPU_GET(member)	(PCPUP->pc_ ## member)

/*
 * XXX The implementation of this operation should be made atomic
 * with respect to preemption.
 */
#define	PCPU_ADD(member, value)	(PCPUP->pc_ ## member += (value))
#define	PCPU_INC(member)	PCPU_ADD(member, 1)
#define	PCPU_PTR(member)	(&PCPUP->pc_ ## member)
#define	PCPU_SET(member,value)	(PCPUP->pc_ ## member = (value))

#endif	/* _KERNEL */

#endif	/* !_MACHINE_PCPU_H_ */
