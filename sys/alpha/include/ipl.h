/*-
 * Copyright (c) 1998 Doug Rabson
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
 *	$Id$
 */

#ifndef _MACHINE_IPL_H_
#define	_MACHINE_IPL_H_

#include <machine/alpha_cpu.h>

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
#define splvm()                _splraise(ALPHA_PSL_IPL_IO)
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

extern void	spl0(void);

/* XXX bogus */
extern		unsigned cpl;	/* current priority level mask */

#endif /* !_MACHINE_MD_VAR_H_ */
