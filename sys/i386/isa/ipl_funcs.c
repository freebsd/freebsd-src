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
 *	$Id: ipl_funcs.c,v 1.12 1998/01/15 07:34:01 gibbs Exp $
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
DO_SETBITS(setsoftcamnet,&ipending, SWI_CAMNET_PENDING)
DO_SETBITS(setsoftcambio,&ipending, SWI_CAMBIO_PENDING)
DO_SETBITS(setsoftclock, &ipending, SWI_CLOCK_PENDING)
DO_SETBITS(setsoftnet,   &ipending, SWI_NET_PENDING)
DO_SETBITS(setsofttty,   &ipending, SWI_TTY_PENDING)
DO_SETBITS(setsoftvm,	 &ipending, SWI_VM_PENDING)

DO_SETBITS(schedsoftcamnet, &idelayed, SWI_CAMNET_PENDING)
DO_SETBITS(schedsoftcambio, &idelayed, SWI_CAMBIO_PENDING)
DO_SETBITS(schedsoftnet, &idelayed, SWI_NET_PENDING)
DO_SETBITS(schedsofttty, &idelayed, SWI_TTY_PENDING)
DO_SETBITS(schedsoftvm,	&idelayed, SWI_VM_PENDING)

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
GENSPL(splcam, cpl |= cam_imask)
GENSPL(splclock, cpl = HWI_MASK | SWI_MASK)
GENSPL(splhigh, cpl = HWI_MASK | SWI_MASK)
GENSPL(splimp, cpl |= net_imask)
GENSPL(splnet, cpl |= SWI_NET_MASK)
GENSPL(splsoftcam, cpl |= SWI_CAMBIO_MASK | SWI_CAMNET_MASK)
GENSPL(splsoftcambio, cpl |= SWI_CAMBIO_MASK)
GENSPL(splsoftcamnet, cpl |= SWI_CAMNET_MASK)
GENSPL(splsoftclock, cpl = SWI_CLOCK_MASK)
GENSPL(splsofttty, cpl |= SWI_TTY_MASK)
GENSPL(splsoftvm, cpl |= SWI_VM_MASK)
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

#include <machine/smp.h>
#include <machine/smptests.h>

#ifndef SPL_DEBUG_POSTCODE
#undef POSTCODE
#undef POSTCODE_LO
#undef POSTCODE_HI
#define POSTCODE(X)
#define POSTCODE_LO(X)
#define POSTCODE_HI(X)
#endif /* SPL_DEBUG_POSTCODE */


/*
 * The volatile bitmap variables must be set atomically.  This normally
 * involves using a machine-dependent bit-set or `or' instruction.
 */

#define DO_SETBITS(name, var, bits)		\
void name(void)					\
{						\
	IFCPL_LOCK();				\
	setbits(var, bits);			\
	IFCPL_UNLOCK();				\
}

DO_SETBITS(setdelayed,   &ipending, loadandclear((unsigned *)&idelayed))
DO_SETBITS(setsoftast,   &ipending, SWI_AST_PENDING)
DO_SETBITS(setsoftcamnet,&ipending, SWI_CAMNET_PENDING)
DO_SETBITS(setsoftcambio,&ipending, SWI_CAMBIO_PENDING)
DO_SETBITS(setsoftclock, &ipending, SWI_CLOCK_PENDING)
DO_SETBITS(setsoftnet,   &ipending, SWI_NET_PENDING)
DO_SETBITS(setsofttty,   &ipending, SWI_TTY_PENDING)
DO_SETBITS(setsoftvm,	 &ipending, SWI_VM_PENDING)

DO_SETBITS(schedsoftcamnet, &idelayed, SWI_CAMNET_PENDING)
DO_SETBITS(schedsoftcambio, &idelayed, SWI_CAMBIO_PENDING)
DO_SETBITS(schedsoftnet, &idelayed, SWI_NET_PENDING)
DO_SETBITS(schedsofttty, &idelayed, SWI_TTY_PENDING)
DO_SETBITS(schedsoftvm,	&idelayed, SWI_VM_PENDING)

unsigned
softclockpending(void)
{
	unsigned x;

	IFCPL_LOCK();
	x = ipending & SWI_CLOCK_PENDING;
	IFCPL_UNLOCK();

	return (x);
}


/*
 * This version has to check for bsp_apic_ready,
 * as calling simple_lock() (ie ss_lock) before then deadlocks the system.
 * A sample count of GENSPL calls before bsp_apic_ready was set: 2193
 */

#ifdef INTR_SPL

#ifdef SPL_DEBUG
#define MAXZ		100000000
#define SPIN_VAR	unsigned z;
#define SPIN_RESET	z = 0;
#if 0
#define SPIN_SPL							\
			if (++z >= MAXZ) {				\
				/* XXX allow lock-free panic */		\
				bsp_apic_ready = 0;			\
				panic("\ncil: 0x%08x", cil);		\
			}
#else
#define SPIN_SPL							\
			if (++z >= MAXZ) {				\
				/* XXX allow lock-free panic */		\
				bsp_apic_ready = 0;			\
				printf("\ncil: 0x%08x", cil);		\
				breakpoint();				\
			}
#endif /* 0/1 */
#else /* SPL_DEBUG */
#define SPIN_VAR
#define SPIN_RESET
#define SPIN_SPL
#endif /* SPL_DEBUG */

#endif

#ifdef INTR_SPL

#define	GENSPL(NAME, OP, MODIFIER, PC)					\
unsigned NAME(void)							\
{									\
	unsigned x, y;							\
	SPIN_VAR;							\
									\
	if (!bsp_apic_ready) {						\
		x = cpl;						\
		cpl OP MODIFIER;					\
		return (x);						\
	}								\
									\
	for (;;) {							\
		IFCPL_LOCK();		/* MP-safe */			\
		x = y = cpl;		/* current value */		\
		POSTCODE(0x20 | PC);					\
		if (inside_intr)					\
			break;		/* XXX only 1 INT allowed */	\
		y OP MODIFIER;		/* desired value */		\
		if (cil & y) {		/* not now */			\
			IFCPL_UNLOCK();	/* allow cil to change */	\
			SPIN_RESET;					\
			while (cil & y)					\
				SPIN_SPL				\
			continue;	/* try again */			\
		}							\
		break;							\
	}								\
	cpl OP MODIFIER;		/* make the change */		\
	IFCPL_UNLOCK();							\
									\
	return (x);							\
}

/*    NAME:            OP:     MODIFIER:				PC: */
GENSPL(splbio,		|=,	bio_imask,				2)
GENSPL(splcam,		|=,	cam_imask,				7)
GENSPL(splclock,	 =,	HWI_MASK | SWI_MASK,			3)
GENSPL(splhigh,		 =,	HWI_MASK | SWI_MASK,			4)
GENSPL(splimp,		|=,	net_imask,				5)
GENSPL(splnet,		|=,	SWI_NET_MASK,				6)
GENSPL(splsoftcam,	|=,	SWI_CAMBIO_MASK | SWI_CAMNET_MASK,	8)
GENSPL(splsoftcambio,	|=,	SWI_CAMBIO_MASK,			9)
GENSPL(splsoftcamnet, 	|=,	SWI_CAMNET_MASK,			10)
GENSPL(splsoftclock,	 =,	SWI_CLOCK_MASK,				11)
GENSPL(splsofttty,	|=,	SWI_TTY_MASK,				12)
GENSPL(splsoftvm,	|=,	SWI_VM_MASK,				16)
GENSPL(splstatclock,	|=,	stat_imask,				13)
GENSPL(spltty,		|=,	tty_imask,				14)
GENSPL(splvm,		|=,	net_imask | bio_imask,			15)

#else /* INTR_SPL */

#define	GENSPL(NAME, set_cpl)			\
unsigned NAME(void)				\
{						\
	unsigned x;				\
						\
	if (!bsp_apic_ready) {			\
		x = cpl;			\
		set_cpl;			\
	}					\
	else {					\
		IFCPL_LOCK();			\
		x = cpl;			\
		set_cpl;			\
		IFCPL_UNLOCK();			\
	}					\
						\
	return (x);				\
}

GENSPL(splbio, cpl |= bio_imask)
GENSPL(splclock, cpl = HWI_MASK | SWI_MASK)
GENSPL(splhigh, cpl = HWI_MASK | SWI_MASK)
GENSPL(splimp, cpl |= net_imask)
GENSPL(splnet, cpl |= SWI_NET_MASK)
GENSPL(splcam, cpl |= cam_imask)
GENSPL(splsoftcam, cpl |= SWI_CAMBIO_MASK | SWI_CAMNET_MASK)
GENSPL(splsoftcambio, cpl |= SWI_CAMBIO_MASK)
GENSPL(splsoftcamnet, cpl |= SWI_CAMNET_MASK)
GENSPL(splsoftclock, cpl = SWI_CLOCK_MASK)
GENSPL(splsofttty, cpl |= SWI_TTY_MASK)
GENSPL(splsoftvm, cpl |= SWI_VM_MASK)
GENSPL(splstatclock, cpl |= stat_imask)
GENSPL(spltty, cpl |= tty_imask)
GENSPL(splvm, cpl |= net_imask | bio_imask)

#endif /* INTR_SPL */


void
spl0(void)
{
	int unpend;
#ifdef INTR_SPL
	SPIN_VAR;

	for (;;) {
		IFCPL_LOCK();
		POSTCODE_HI(0xc);
		if (cil & SWI_AST_MASK) {	/* not now */
			IFCPL_UNLOCK();		/* allow cil to change */
			SPIN_RESET;
			while (cil & SWI_AST_MASK)
				SPIN_SPL
			continue;		/* try again */
		}
		break;
	}
#else /* INTR_SPL */
	IFCPL_LOCK();
#endif /* INTR_SPL */

	cpl = SWI_AST_MASK;
	unpend = ipending & ~SWI_AST_MASK;
	IFCPL_UNLOCK();

	if (unpend && !inside_intr)
		splz();
}

void
splx(unsigned ipl)
{
	int unpend;
#ifdef INTR_SPL
	SPIN_VAR;
#endif

	if (!bsp_apic_ready) {
		cpl = ipl;
		if (ipending & ~ipl)
			splz();
		return;
	}

#ifdef INTR_SPL
	for (;;) {
		IFCPL_LOCK();
		POSTCODE_HI(0xe);
		if (inside_intr)
			break;			/* XXX only 1 INT allowed */
		POSTCODE_HI(0xf);
		if (cil & ipl) {		/* not now */
			IFCPL_UNLOCK();		/* allow cil to change */
			SPIN_RESET;
			while (cil & ipl)
				SPIN_SPL
			continue;		/* try again */
		}
		break;
	}
#else /* INTR_SPL */
	IFCPL_LOCK();
#endif /* INTR_SPL */

	cpl = ipl;
	unpend = ipending & ~ipl;
	IFCPL_UNLOCK();

	if (unpend && !inside_intr)
		splz();
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
	intrmask_t tmp, tmp2;

#ifdef INTR_SPL
	for (;;) {
		IFCPL_LOCK();
		tmp = tmp2 = cpl;
		tmp2 |= mask;
		if (cil & tmp2) {		/* not now */
			IFCPL_UNLOCK();		/* allow cil to change */
			while (cil & tmp2)
				/* spin */ ;
			continue;		/* try again */
		}
		break;
	}
	cpl = tmp2;
#else /* INTR_SPL */
	IFCPL_LOCK();
	tmp = cpl;
	cpl |= mask;
#endif /* INTR_SPL */

	IFCPL_UNLOCK();
	return (tmp);
}

#endif /* !SMP */
