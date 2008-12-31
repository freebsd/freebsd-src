/*	$NetBSD: ixp425_intr.h,v 1.6 2005/12/24 20:06:52 perry Exp $	*/

/*
 * Copyright (c) 2001, 2002 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Jason R. Thorpe for Wasabi Systems, Inc.
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
 *	This product includes software developed for the NetBSD Project by
 *	Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/arm/xscale/ixp425/ixp425_intr.h,v 1.1.8.1 2008/11/25 02:59:29 kensmith Exp $
 *
 */

#ifndef _IXP425_INTR_H_
#define _IXP425_INTR_H_

#define	ARM_IRQ_HANDLER	_C_LABEL(ixp425_intr_dispatch)

#ifndef _LOCORE

#include <machine/armreg.h>
#include <machine/cpufunc.h>

#include <arm/xscale/ixp425/ixp425reg.h>

#define IXPREG(reg)     *((__volatile u_int32_t*) (reg))

void ixp425_do_pending(void);

extern __volatile uint32_t intr_enabled;
extern uint32_t intr_steer;

static __inline void __attribute__((__unused__))
ixp425_set_intrmask(void)
{
	IXPREG(IXP425_INT_ENABLE) = intr_enabled & IXP425_INT_HWMASK;
}

static __inline void
ixp425_set_intrsteer(void)
{
	IXPREG(IXP425_INT_SELECT) = intr_steer & IXP425_INT_HWMASK;
}

#define INT_SWMASK						\
	((1U << IXP425_INT_bit31) | (1U << IXP425_INT_bit30) |	\
	 (1U << IXP425_INT_bit14) | (1U << IXP425_INT_bit11))

#if 0
static __inline void __attribute__((__unused__))
ixp425_splx(int new)
{
	extern __volatile uint32_t intr_enabled;
	extern __volatile int current_spl_level;
	extern __volatile int ixp425_ipending;
	extern void ixp425_do_pending(void);
	int oldirqstate, hwpend;

	/* Don't let the compiler re-order this code with preceding code */
	__insn_barrier();

	current_spl_level = new;

	hwpend = (ixp425_ipending & IXP425_INT_HWMASK) & ~new;
	if (hwpend != 0) {
		oldirqstate = disable_interrupts(I32_bit);
		intr_enabled |= hwpend;
		ixp425_set_intrmask();
		restore_interrupts(oldirqstate);
	}

	if ((ixp425_ipending & INT_SWMASK) & ~new)
		ixp425_do_pending();
}

static __inline int __attribute__((__unused__))
ixp425_splraise(int ipl)
{
	extern __volatile int current_spl_level;
	extern int ixp425_imask[];
	int	old;

	old = current_spl_level;
	current_spl_level |= ixp425_imask[ipl];

	/* Don't let the compiler re-order this code with subsequent code */
	__insn_barrier();

	return (old);
}

static __inline int __attribute__((__unused__))
ixp425_spllower(int ipl)
{
	extern __volatile int current_spl_level;
	extern int ixp425_imask[];
	int old = current_spl_level;

	ixp425_splx(ixp425_imask[ipl]);
	return(old);
}

#endif
#if !defined(EVBARM_SPL_NOINLINE)

#define splx(new)		ixp425_splx(new)
#define	_spllower(ipl)		ixp425_spllower(ipl)
#define	_splraise(ipl)		ixp425_splraise(ipl)
void	_setsoftintr(int);

#else

int	_splraise(int);
int	_spllower(int);
void	splx(int);
void	_setsoftintr(int);

#endif /* ! EVBARM_SPL_NOINLINE */

#endif /* _LOCORE */

#endif /* _IXP425_INTR_H_ */
