/*-
 * Copyright (c) 1997 Bruce Evans.
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
 *	$Id: ipl_funcs.c,v 1.3 1997/08/24 00:05:18 fsmp Exp $
 */

#include <sys/types.h>
#include <sys/systm.h>
#include <machine/ipl.h>

#ifndef SMP
/*
 * The volatile bitmap variables must be set atomically.  This normally
 * involves using a machine-dependent bit-set or `or' instruction.
 */

#define DO_SETBITS(name, var, bits) \
void name(void)					\
{						\
	setbits(var, bits);			\
}

DO_SETBITS(setdelayed,   &ipending, loadandclear((unsigned *)&idelayed))
DO_SETBITS(setsoftast,   &ipending, SWI_AST_PENDING)
DO_SETBITS(setsoftclock, &ipending, SWI_CLOCK_PENDING)
DO_SETBITS(setsoftnet,   &ipending, SWI_NET_PENDING)
DO_SETBITS(setsofttty,   &ipending, SWI_TTY_PENDING)

DO_SETBITS(schedsoftnet, &idelayed, SWI_NET_PENDING)
DO_SETBITS(schedsofttty, &idelayed, SWI_TTY_PENDING)

unsigned
softclockpending(void)
{
	return (ipending & SWI_CLOCK_PENDING);
}

#define	GENSPL(name, set_cpl) \
unsigned name(void)				\
{						\
	unsigned x;				\
						\
	x = cpl;				\
	set_cpl;				\
	return (x);				\
}

GENSPL(splbio, cpl |= bio_imask)
GENSPL(splclock, cpl = HWI_MASK | SWI_MASK)
GENSPL(splhigh, cpl = HWI_MASK | SWI_MASK)
GENSPL(splimp, cpl |= net_imask)
GENSPL(splnet, cpl |= SWI_NET_MASK)
GENSPL(splsoftclock, cpl = SWI_CLOCK_MASK)
GENSPL(splsofttty, cpl |= SWI_TTY_MASK)
GENSPL(splstatclock, cpl |= stat_imask)
GENSPL(spltty, cpl |= tty_imask)
GENSPL(splvm, cpl |= net_imask | bio_imask)

void
spl0(void)
{
	cpl = SWI_AST_MASK;
	if (ipending & ~SWI_AST_MASK)
		splz();
}

void
splx(unsigned ipl)
{
	cpl = ipl;
	if (ipending & ~ipl)
		splz();
}

#else /* !SMP */

#include <machine/param.h>
#include <machine/smp.h>

#if defined(REAL_IFCPL)

#define IFCPL_LOCK()		SCPL_LOCK()
#define IFCPL_UNLOCK()		SCPL_UNLOCK()

#else /* REAL_IFCPL */

#define IFCPL_LOCK()
#define IFCPL_UNLOCK()

#endif /* REAL_IFCPL */

/*
 * The volatile bitmap variables must be set atomically.  This normally
 * involves using a machine-dependent bit-set or `or' instruction.
 */

#define DO_SETBITS(name, var, bits) \
void name(void)					\
{						\
	IFCPL_LOCK();				\
	setbits(var, bits);			\
	IFCPL_UNLOCK();				\
}

DO_SETBITS(setdelayed,   &ipending, loadandclear((unsigned *)&idelayed))
DO_SETBITS(setsoftast,   &ipending, SWI_AST_PENDING)
DO_SETBITS(setsoftclock, &ipending, SWI_CLOCK_PENDING)
DO_SETBITS(setsoftnet,   &ipending, SWI_NET_PENDING)
DO_SETBITS(setsofttty,   &ipending, SWI_TTY_PENDING)

DO_SETBITS(schedsoftnet, &idelayed, SWI_NET_PENDING)
DO_SETBITS(schedsofttty, &idelayed, SWI_TTY_PENDING)

unsigned
softclockpending(void)
{
	unsigned x;

	IFCPL_LOCK();
	x = ipending & SWI_CLOCK_PENDING;
	IFCPL_UNLOCK();

	return x;
}


#define	GENSPL(name, set_cpl)			\
unsigned name(void)				\
{						\
	unsigned x;				\
						\
	IFCPL_LOCK();				\
	x = cpl;				\
	/* XXX test cil */			\
	set_cpl;				\
	IFCPL_UNLOCK();				\
						\
	return (x);				\
}

/*
 * This version has to check for smp_active,
 * as calling simple_lock() (ie ss_lock) before then deadlocks the system.
 */
#define	GENSPL2(name, set_cpl)			\
unsigned name(void)				\
{						\
	unsigned x;				\
						\
	if (smp_active)				\
		IFCPL_LOCK();			\
	x = cpl;				\
	/* XXX test cil */			\
	set_cpl;				\
	if (smp_active)				\
		IFCPL_UNLOCK();			\
						\
	return (x);				\
}

GENSPL2(splbio, cpl |= bio_imask)
GENSPL2(splclock, cpl = HWI_MASK | SWI_MASK)
GENSPL2(splimp, cpl |= net_imask)
GENSPL2(splnet, cpl |= SWI_NET_MASK)
GENSPL2(splsoftclock, cpl = SWI_CLOCK_MASK)
GENSPL2(splsofttty, cpl |= SWI_TTY_MASK)
GENSPL2(splstatclock, cpl |= stat_imask)
GENSPL2(splvm, cpl |= net_imask | bio_imask)

GENSPL2(splhigh, cpl = HWI_MASK | SWI_MASK)
GENSPL2(spltty, cpl |= tty_imask)


void
spl0(void)
{
	IFCPL_LOCK();

	/* XXX test cil */
	cpl = SWI_AST_MASK;
	if (ipending & ~SWI_AST_MASK) {
		IFCPL_UNLOCK();
		splz();
	}
	else
		IFCPL_UNLOCK();
}

void
splx(unsigned ipl)
{
	if (smp_active)
		IFCPL_LOCK();

	/* XXX test cil */
	cpl = ipl;
	if (ipending & ~ipl) {
		if (smp_active)
			IFCPL_UNLOCK();
		splz();
	}
	else
		if (smp_active)
			IFCPL_UNLOCK();
}


/*
 * Replaces UP specific inline found in (?) pci/pci_support.c.
 *
 * Stefan said:
 * You know, that splq() is used in the shared interrupt multiplexer, and that
 * the SMP version should not have too much overhead. If it is significantly
 * slower, then moving the splq() out of the loop in intr_mux() and passing in
 * the logical OR of all mask values might be a better solution than the
 * current code. (This logical OR could of course be pre-calculated whenever
 * another shared interrupt is registered ...)
 */
intrmask_t
splq(intrmask_t mask)
{
	intrmask_t tmp;

	IFCPL_LOCK();

	tmp = cpl;
	cpl |= mask;

	IFCPL_UNLOCK();

	return (tmp);
}

#endif /* !SMP */
