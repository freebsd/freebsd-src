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
 * $FreeBSD: src/sys/i386/isa/ipl_funcs.c,v 1.32.2.3 2000/07/18 21:12:42 dfr Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <machine/ipl.h>
#include <machine/globals.h>
#include <i386/isa/intr_machdep.h>

/*
 * Bits in the ipending bitmap variable must be set atomically because
 * ipending may be manipulated by interrupts or other cpu's without holding 
 * any locks.
 *
 * Note: setbits uses a locked or, making simple cases MP safe.
 */
#define DO_SETBITS(name, var, bits) \
void name(void)					\
{						\
	atomic_set_int(var, bits);		\
}

DO_SETBITS(setdelayed,   &ipending, loadandclear(&idelayed))

DO_SETBITS(setsoftcamnet,&ipending, SWI_CAMNET_PENDING)
DO_SETBITS(setsoftcambio,&ipending, SWI_CAMBIO_PENDING)
DO_SETBITS(setsoftclock, &ipending, SWI_CLOCK_PENDING)
DO_SETBITS(setsoftnet,   &ipending, SWI_NET_PENDING)
DO_SETBITS(setsofttty,   &ipending, SWI_TTY_PENDING)
DO_SETBITS(setsoftvm,	 &ipending, SWI_VM_PENDING)
DO_SETBITS(setsofttq,	 &ipending, SWI_TQ_PENDING)

DO_SETBITS(schedsoftcamnet, &idelayed, SWI_CAMNET_PENDING)
DO_SETBITS(schedsoftcambio, &idelayed, SWI_CAMBIO_PENDING)
DO_SETBITS(schedsoftnet, &idelayed, SWI_NET_PENDING)
DO_SETBITS(schedsofttty, &idelayed, SWI_TTY_PENDING)
DO_SETBITS(schedsoftvm,	&idelayed, SWI_VM_PENDING)
DO_SETBITS(schedsofttq,	&idelayed, SWI_TQ_PENDING)

unsigned
softclockpending(void)
{
	return (ipending & SWI_CLOCK_PENDING);
}

/*
 * Support for SPL assertions.
 */

#ifdef INVARIANT_SUPPORT

#define	SPLASSERT_IGNORE	0
#define	SPLASSERT_LOG		1
#define	SPLASSERT_PANIC		2

static int splassertmode = SPLASSERT_LOG;
SYSCTL_INT(_kern, OID_AUTO, splassertmode, CTLFLAG_RW,
	&splassertmode, 0, "Set the mode of SPLASSERT");

static void
init_splassertmode(void *ignored)
{
	TUNABLE_INT_FETCH("kern.splassertmode", 0, splassertmode);
}
SYSINIT(param, SI_SUB_TUNABLES, SI_ORDER_ANY, init_splassertmode, NULL);

static void
splassertfail(char *str, const char *msg, char *name, int level)
{
	switch (splassertmode) {
	case SPLASSERT_IGNORE:
		break;
	case SPLASSERT_LOG:
		printf(str, msg, name, level);
		printf("\n");
		break;
	case SPLASSERT_PANIC:
		panic(str, msg, name, level);
		break;
	}
}

#define	GENSPLASSERT(NAME, MODIFIER)			\
void							\
NAME##assert(const char *msg)				\
{							\
	if ((cpl & (MODIFIER)) != (MODIFIER))		\
		splassertfail("%s: not %s, cpl == %#x",	\
		    msg, __XSTRING(NAME) + 3, cpl);	\
}
#else
#define	GENSPLASSERT(NAME, MODIFIER)
#endif

/************************************************************************
 *			GENERAL SPL CODE				*
 ************************************************************************
 *
 *  Implement splXXX(), spl0(), splx(), and splq().  splXXX() disables a
 *  set of interrupts (e.g. splbio() disables interrupts relating to 
 *  device I/O) and returns the previous interrupt mask.  splx() restores
 *  the previous interrupt mask, spl0() is a special case which enables
 *  all interrupts and is typically used inside i386/i386 swtch.s and
 *  fork_trampoline.  splq() is a generic version of splXXX().
 *
 *  The SPL routines mess around with the 'cpl' global, which masks 
 *  interrupts.  Interrupts are not *actually* masked.  What happens is 
 *  that if an interrupt masked by the cpl occurs, the appropriate bit
 *  in 'ipending' is set and the interrupt is defered.  When we clear
 *  bits in the cpl we must check to see if any ipending interrupts have
 *  been unmasked and issue the synchronously, which is what the splz()
 *  call does.
 *
 *  Because the cpl is often saved and restored in a nested fashion, cpl
 *  modifications are only allowed in the SMP case when the MP lock is held
 *  to prevent multiple processes from tripping over each other's masks.
 *  The cpl is saved when you do a context switch (mi_switch()) and restored
 *  when your process gets cpu again.
 *
 *  An interrupt routine is allowed to modify the cpl as long as it restores
 *  it prior to returning (thus the interrupted mainline code doesn't notice
 *  anything amiss).  For the SMP case, the interrupt routine must hold 
 *  the MP lock for any cpl manipulation.
 *
 *  Likewise, due to the deterministic nature of cpl modifications, we do
 *  NOT need to use locked instructions to modify it.
 */

#ifndef SMP

#define	GENSPL(NAME, OP, MODIFIER, PC)		\
GENSPLASSERT(NAME, MODIFIER)			\
unsigned NAME(void)				\
{						\
	unsigned x;				\
						\
	x = cpl;				\
	cpl OP MODIFIER;			\
	return (x);				\
}

void
spl0(void)
{
	cpl = 0;
	if (ipending)
		splz();
}

void
splx(unsigned ipl)
{
	cpl = ipl;
	if (ipending & ~ipl)
		splz();
}

intrmask_t
splq(intrmask_t mask)
{ 
	intrmask_t tmp = cpl;
	cpl |= mask;
	return (tmp);
}       

#else /* !SMP */

#include <machine/smp.h>
#include <machine/smptests.h>

/*
 *	SMP CASE
 *
 *	Mostly the same as the non-SMP case now, but it didn't used to be
 *	this clean.
 */

#define	GENSPL(NAME, OP, MODIFIER, PC)		\
GENSPLASSERT(NAME, MODIFIER)			\
unsigned NAME(void)				\
{						\
	unsigned x;				\
						\
	x = cpl;				\
	cpl OP MODIFIER;			\
						\
	return (x);				\
}

/*
 * spl0() -	unmask all interrupts
 *
 *	The MP lock must be held on entry
 *	This routine may only be called from mainline code.
 */
void
spl0(void)
{
	KASSERT(inside_intr == 0, ("spl0: called from interrupt"));
	cpl = 0;
	if (ipending)
		splz();
}

/*
 * splx() -	restore previous interrupt mask
 *
 *	The MP lock must be held on entry
 */

void
splx(unsigned ipl)
{
	cpl = ipl;
	if (inside_intr == 0 && (ipending & ~cpl) != 0)
		splz();
}


/*
 * splq() -	blocks specified interrupts
 *
 *	The MP lock must be held on entry
 */
intrmask_t
splq(intrmask_t mask)
{
	intrmask_t tmp = cpl;
	cpl |= mask;
	return (tmp);
}

#endif /* !SMP */

/* Finally, generate the actual spl*() functions */

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
GENSPL(splsofttq,	|=,	SWI_TQ_MASK,				17)
GENSPL(splstatclock,	|=,	stat_imask,				13)
GENSPL(spltty,		|=,	tty_imask,				14)
GENSPL(splvm,		|=,	net_imask | bio_imask | cam_imask,	15)
