/*-
 * Copyright (c) 1993 The Regents of the University of California.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$Id: ipl.h,v 1.12 1997/09/21 21:38:53 gibbs Exp $
 */

#ifndef _MACHINE_IPL_H_
#define	_MACHINE_IPL_H_

#if defined(KERNEL) && !defined(ACTUALLY_LKM_NOT_KERNEL)

#ifdef APIC_IO
#include <i386/isa/apic_ipl.h>
#else
#include <i386/isa/icu_ipl.h>
#endif

/*
 * Software interrupt bit numbers in priority order.  The priority only
 * determines which swi will be dispatched next; a higher priority swi
 * may be dispatched when a nested h/w interrupt handler returns.
 */
#define	SWI_TTY		(NHWI + 0)
#define	SWI_NET		(NHWI + 1)
#define	SWI_CAMNET	(NHWI + 2)
#define	SWI_CAMBIO	(NHWI + 3)
#define	SWI_VM		(NHWI + 4)
#define	SWI_CLOCK	30
#define	SWI_AST		31

/*
 * Corresponding interrupt-pending bits for ipending.
 */
#define	SWI_TTY_PENDING		(1 << SWI_TTY)
#define	SWI_NET_PENDING		(1 << SWI_NET)
#define	SWI_CAMNET_PENDING	(1 << SWI_CAMNET)
#define	SWI_CAMBIO_PENDING	(1 << SWI_CAMBIO)
#define	SWI_VM_PENDING		(1 << SWI_VM)
#define	SWI_CLOCK_PENDING	(1 << SWI_CLOCK)
#define	SWI_AST_PENDING		(1 << SWI_AST)

/*
 * Corresponding interrupt-disable masks for cpl.  The ordering is now by
 * inclusion (where each mask is considered as a set of bits). Everything
 * except SWI_AST_MASK includes SWI_CLOCK_MASK so that softclock() doesn't
 * run while other swi handlers are running and timeout routines can call
 * swi handlers.  Everything includes SWI_AST_MASK so that AST's are masked
 * until just before return to user mode.  SWI_TTY_MASK includes SWI_NET_MASK
 * in case tty interrupts are processed at splsofttty() for a tty that is in
 * SLIP or PPP line discipline (this is weaker than merging net_imask with
 * tty_imask in isa.c - splimp() must mask hard and soft tty interrupts, but
 * spltty() apparently only needs to mask soft net interrupts).
 */
#define	SWI_TTY_MASK	(SWI_TTY_PENDING | SWI_CLOCK_MASK | SWI_NET_MASK)
#define	SWI_CAMNET_MASK	(SWI_CAMNET_PENDING | SWI_CLOCK_MASK)
#define	SWI_CAMBIO_MASK	(SWI_CAMBIO_PENDING | SWI_CLOCK_MASK)
#define	SWI_NET_MASK	(SWI_NET_PENDING | SWI_CLOCK_MASK)
#define	SWI_VM_MASK	(SWI_VM_PENDING | SWI_CLOCK_MASK)
#define	SWI_CLOCK_MASK	(SWI_CLOCK_PENDING | SWI_AST_MASK)
#define	SWI_AST_MASK	SWI_AST_PENDING
#define	SWI_MASK	(~HWI_MASK)

#endif /* KERNEL && !ACTUALLY_LKM_NOT_KERNEL */

#ifndef	LOCORE

/*
 * cpl is preserved by interrupt handlers so it is effectively nonvolatile.
 * ipending and idelayed are changed by interrupt handlers so they are
 * volatile.
 */
#ifdef notyet /* in <sys/interrupt.h> until pci drivers stop hacking on them */
extern	unsigned bio_imask;	/* group of interrupts masked with splbio() */
#endif
extern	unsigned cpl;		/* current priority level mask */
#ifdef SMP
extern	unsigned cil;		/* current INTerrupt level mask */
#endif
extern	volatile unsigned idelayed;	/* interrupts to become pending */
extern	volatile unsigned ipending;	/* active interrupts masked by cpl */
#ifdef notyet /* in <sys/interrupt.h> until pci drivers stop hacking on them */
extern	unsigned net_imask;	/* group of interrupts masked with splimp() */
extern	unsigned stat_imask;	/* interrupts masked with splstatclock() */
extern	unsigned tty_imask;	/* group of interrupts masked with spltty() */
#endif

#endif /* !LOCORE */

#endif /* !_MACHINE_IPL_H_ */
