/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 *	$NetBSD: intr.h,v 1.6 2000/02/11 13:15:44 tsubai Exp $
 * $FreeBSD$
 */

#ifndef _MACPPC_INTR_H_
#define	_MACPPC_INTR_H_

/* Interrupt priority `levels'. */
#define	IPL_NONE	9	/* nothing */
#define	IPL_SOFTCLOCK	8	/* timeouts */
#define	IPL_SOFTNET	7	/* protocol stacks */
#define	IPL_BIO		6	/* block I/O */
#define	IPL_NET		5	/* network */
#define	IPL_SOFTSERIAL	4	/* serial */
#define	IPL_TTY		3	/* terminal */
#define	IPL_IMP		3	/* memory allocation */
#define	IPL_AUDIO	2	/* audio */
#define	IPL_CLOCK	1	/* clock */
#define	IPL_HIGH	1	/* everything */
#define	IPL_SERIAL	0	/* serial */
#define	NIPL		10

/* Interrupt sharing types. */
#define	IST_NONE	0	/* none */
#define	IST_PULSE	1	/* pulsed */
#define	IST_EDGE	2	/* edge-triggered */
#define	IST_LEVEL	3	/* level-triggered */

#ifndef LOCORE

#if 0
/*
 * Interrupt handler chains.  intr_establish() inserts a handler into
 * the list.  The handler is called with its (single) argument.
 */
struct intrhand {
	int	(*ih_fun)(void *);
	void	*ih_arg;
	u_long	ih_count;
	struct	intrhand *ih_next;
	int	ih_level;
	int	ih_irq;
};
#endif

void	setsoftclock(void);
void	clearsoftclock(void);
void	setsoftnet(void);
void	clearsoftnet(void);

void	do_pending_int(void);

static __inline void softintr(int);

extern u_int cpl, ipending, tickspending;
extern int imask[];

/* Following code should be implemented with lwarx/stwcx to avoid
 * the disable/enable. i need to read the manual once more.... */
static __inline void
softintr(int ipl)
{
	unsigned int	msrsave;

	msrsave = mfmsr();
	mtmsr(msrsave & ~PSL_EE);

	ipending |= 1 << ipl;

	mtmsr(msrsave);
}

#define	ICU_LEN		64

/* Soft interrupt masks. */
#define	SIR_CLOCK	28
#define	SIR_NET		29
#define	SIR_SERIAL	30
#define	SPL_CLOCK	31

#if 0

/*
 * Hardware interrupt masks
 */

#define	splbio()	splraise(imask[IPL_BIO])
#define	splnet()	splraise(imask[IPL_NET])
#define	spltty()	splraise(imask[IPL_TTY])
#define	splaudio()	splraise(imask[IPL_AUDIO])
#define	splclock()	splraise(imask[IPL_CLOCK])
#define	splstatclock()	splclock()
#define	splserial()	splraise(imask[IPL_SERIAL])

#define	spllpt()	spltty()

/*
 * Software interrupt masks
 *
 * NOTE: splsoftclock() is used by hardclock() to lower the priority from
 * clock to softclock before it calls softclock().
 */
#define	spllowersoftclock() spllower(imask[IPL_SOFTCLOCK])
#define	splsoftclock()	splraise(imask[IPL_SOFTCLOCK])
#define	splsoftnet()	splraise(imask[IPL_SOFTNET])
#define	splsoftserial()	splraise(imask[IPL_SOFTSERIAL])

/*
 * Miscellaneous
 */
#define	splimp()	splraise(imask[IPL_IMP])
#define	splhigh()	splraise(imask[IPL_HIGH])
#define	spl0()		spllower(0)

#endif /* 0 */

#define	setsoftclock()	softintr(SIR_CLOCK)
#define	setsoftnet()	softintr(SIR_NET)
#define	setsoftserial()	softintr(SIR_SERIAL)

#define	CNT_IRQ0	0
#define	CNT_CLOCK	64
#define	CNT_SOFTCLOCK	65
#define	CNT_SOFTNET	66
#define	CNT_SOFTSERIAL	67

#endif /* !LOCORE */

#endif /* !_MACPPC_INTR_H_ */
