/*	$NetBSD: i80321_intr.h,v 1.5 2004/01/12 10:25:06 scw Exp $	*/

/*-
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
 * $FreeBSD$
 *
 */

#ifndef _I80321_INTR_H_
#define _I80321_INTR_H_

#define	ARM_IRQ_HANDLER	_C_LABEL(i80321_intr_dispatch)

#ifndef _LOCORE

#include <machine/armreg.h>
#include <machine/cpufunc.h>

#include <arm/xscale/i80321/i80321reg.h>

void i80321_do_pending(void);

extern __volatile uint32_t intr_enabled;
extern uint32_t intr_steer;

static __inline void __attribute__((__unused__))
i80321_set_intrmask(void)
{

	__asm __volatile("mcr p6, 0, %0, c0, c0, 0"
		:
		: "r" (intr_enabled & ICU_INT_HWMASK));
}

static __inline void
i80321_set_intrsteer(void)
{

	__asm __volatile("mcr p6, 0, %0, c4, c0, 0"
	    :
	    : "r" (intr_steer & ICU_INT_HWMASK));
}

#if defined ( CPU_XSCALE_80219 )
#define INT_SWMASK														\
	((1U << ICU_INT_bit26) |											\
	 (1U << ICU_INT_bit25) |											\
	 (1U << ICU_INT_bit23) |											\
	 (1U << ICU_INT_bit22) |											\
	 (1U << ICU_INT_bit7)  |											\
	 (1U << ICU_INT_bit6)  |											\
	 (1U << ICU_INT_bit5)  |											\
	 (1U << ICU_INT_bit4))
#else
#define INT_SWMASK                                                      \
        ((1U << ICU_INT_bit26) | (1U << ICU_INT_bit22) |                \
         (1U << ICU_INT_bit5)  | (1U << ICU_INT_bit4))
#endif

#if 0
static __inline void __attribute__((__unused__))
i80321_splx(int new)
{
	extern __volatile uint32_t intr_enabled;
	extern __volatile int current_spl_level;
	extern __volatile int i80321_ipending;
	extern void i80321_do_pending(void);
	int oldirqstate, hwpend;

	/* Don't let the compiler re-order this code with preceding code */
	__insn_barrier();

	current_spl_level = new;

	hwpend = (i80321_ipending & ICU_INT_HWMASK) & ~new;
	if (hwpend != 0) {
		oldirqstate = disable_interrupts(I32_bit);
		intr_enabled |= hwpend;
		i80321_set_intrmask();
		restore_interrupts(oldirqstate);
	}

	if ((i80321_ipending & INT_SWMASK) & ~new)
		i80321_do_pending();
}

static __inline int __attribute__((__unused__))
i80321_splraise(int ipl)
{
	extern __volatile int current_spl_level;
	extern int i80321_imask[];
	int	old;

	old = current_spl_level;
	current_spl_level |= i80321_imask[ipl];

	/* Don't let the compiler re-order this code with subsequent code */
	__insn_barrier();

	return (old);
}

static __inline int __attribute__((__unused__))
i80321_spllower(int ipl)
{
	extern __volatile int current_spl_level;
	extern int i80321_imask[];
	int old = current_spl_level;

	i80321_splx(i80321_imask[ipl]);
	return(old);
}

#endif
#if !defined(EVBARM_SPL_NOINLINE)

#define splx(new)		i80321_splx(new)
#define	_spllower(ipl)		i80321_spllower(ipl)
#define	_splraise(ipl)		i80321_splraise(ipl)
void	_setsoftintr(int);

#else

int	_splraise(int);
int	_spllower(int);
void	splx(int);
void	_setsoftintr(int);

#endif /* ! EVBARM_SPL_NOINLINE */

#endif /* _LOCORE */

#endif /* _I80321_INTR_H_ */
