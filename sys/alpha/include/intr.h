/* $Id$ */
/* From: NetBSD: intr.h,v 1.11 1997/11/10 18:23:50 mjacob Exp */

/*
 * Copyright (c) 1997 Christopher G. Demetriou.  All rights reserved.
 * Copyright (c) 1996 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 *
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#ifndef _ALPHA_INTR_H_
#define _ALPHA_INTR_H_

#include <sys/queue.h>

#define	IPL_NONE	0	/* disable only this interrupt */
#define	IPL_BIO		1	/* disable block I/O interrupts */
#define	IPL_NET		2	/* disable network interrupts */
#define	IPL_TTY		3	/* disable terminal interrupts */
#define	IPL_CLOCK	4	/* disable clock interrupts */
#define	IPL_HIGH	5	/* disable all interrupts */

#define	IST_UNUSABLE	-1	/* interrupt cannot be used */
#define	IST_NONE	0	/* none (dummy) */
#define	IST_PULSE	1	/* pulsed */
#define	IST_EDGE	2	/* edge-triggered */
#define	IST_LEVEL	3	/* level-triggered */
#ifdef	_KERNEL

/* IPL-lowering/restoring macros */
#define splx(s)								\
    ((s) == ALPHA_PSL_IPL_0 ? spl0() : alpha_pal_swpipl(s))
#define splsoft()               alpha_pal_swpipl(ALPHA_PSL_IPL_SOFT)
#define splsoftclock()          splsoft()
#define splsoftnet()            splsoft()

/* IPL-raising functions/macros */
static __inline int _splraise __P((int)) __attribute__ ((unused));
static __inline int
_splraise(s)
	int s;
{
	int cur = alpha_pal_rdps() & ALPHA_PSL_IPL_MASK;
	return (s > cur ? alpha_pal_swpipl(s) : cur);
}
#define splnet()                _splraise(ALPHA_PSL_IPL_IO)
#define splbio()                _splraise(ALPHA_PSL_IPL_IO)
#define splimp()                _splraise(ALPHA_PSL_IPL_IO)
#define spltty()                _splraise(ALPHA_PSL_IPL_IO)
#define splclock()              _splraise(ALPHA_PSL_IPL_CLOCK)
#define splstatclock()          _splraise(ALPHA_PSL_IPL_CLOCK)
#define splhigh()               _splraise(ALPHA_PSL_IPL_HIGH)

/*
 * simulated software interrupt register
 */
extern u_int64_t ssir;

#define	SIR_NET		0x1
#define	SIR_CLOCK	0x2

#define	setsoftnet()	ssir |= SIR_NET
#define	setsoftclock()	ssir |= SIR_CLOCK

/*
 * Alpha shared-interrupt-line common code.
 */

struct alpha_shared_intrhand {
	TAILQ_ENTRY(alpha_shared_intrhand)
		ih_q;
	int	(*ih_fn) __P((void *));
	void	*ih_arg;
	int	ih_level;
};

struct alpha_shared_intr {
	TAILQ_HEAD(,alpha_shared_intrhand)
		intr_q;
	int	intr_sharetype;
	int	intr_dfltsharetype;
	int	intr_nstrays;
	int	intr_maxstrays;
};

struct alpha_shared_intr *alpha_shared_intr_alloc __P((unsigned int));
int	alpha_shared_intr_dispatch __P((struct alpha_shared_intr *,
	    unsigned int));
void	*alpha_shared_intr_establish __P((struct alpha_shared_intr *,
	    unsigned int, int, int, int (*)(void *), void *, const char *));
int	alpha_shared_intr_get_sharetype __P((struct alpha_shared_intr *,
	    unsigned int));
int	alpha_shared_intr_isactive __P((struct alpha_shared_intr *,
	    unsigned int));
void	alpha_shared_intr_set_dfltsharetype __P((struct alpha_shared_intr *,
	    unsigned int, int));
void	alpha_shared_intr_set_maxstrays __P((struct alpha_shared_intr *,
	    unsigned int, int));
void	alpha_shared_intr_stray __P((struct alpha_shared_intr *, unsigned int,
	    const char *));

#endif
#endif
