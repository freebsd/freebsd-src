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
 * $FreeBSD$
 */

#ifndef _MACHINE_IPL_H_
#define	_MACHINE_IPL_H_


#include <machine/cpu.h> 	/* for pal inlines */

/*
 * Software interrupt bit numbers
 */
#define SWI_TTY		0
#define SWI_NET		1
#define SWI_CAMNET	2
#define SWI_CAMBIO	3
#define SWI_VM		4
#define SWI_CLOCK	5
#define SWI_TQ		6
#define NSWI		32
#define NHWI		0

extern u_int32_t ipending;

#define getcpl()	(alpha_pal_rdps() & ALPHA_PSL_IPL_MASK)

#define SPLDOWN(name, pri)			\
						\
static __inline int name(void)			\
{						\
    int s;					\
    s = alpha_pal_swpipl(ALPHA_PSL_IPL_##pri);	\
    return s;					\
}

SPLDOWN(splsoftclock, SOFT)
SPLDOWN(splsoft, SOFT)

#define SPLUP(name, pri)				\
							\
static __inline int name(void)				\
{							\
    int cpl = getcpl();					\
    if (ALPHA_PSL_IPL_##pri > cpl) {			\
	int s = alpha_pal_swpipl(ALPHA_PSL_IPL_##pri);	\
	return s;					\
    } else						\
	return cpl;					\
}

SPLUP(splsoftcam, SOFT)
SPLUP(splsoftnet, SOFT)
SPLUP(splsoftvm, SOFT)
SPLUP(splsofttq, SOFT)
SPLUP(splnet, IO)
SPLUP(splbio, IO)
SPLUP(splcam, IO)
SPLUP(splimp, IO)
SPLUP(spltty, IO)
SPLUP(splvm, IO)
SPLUP(splclock, CLOCK)
SPLUP(splstatclock, CLOCK)
SPLUP(splhigh, HIGH)

static __inline void
spl0(void)
{
    if (ipending)
	do_sir();		/* lowers ipl to SOFT */

    alpha_pal_swpipl(ALPHA_PSL_IPL_0);
}

static __inline void
splx(int s)
{
    if (s)
	alpha_pal_swpipl(s);
    else
	spl0();
}

extern void setdelayed(void);
extern void setsofttty(void);
extern void setsoftnet(void);
extern void setsoftcamnet(void);
extern void setsoftcambio(void);
extern void setsoftvm(void);
extern void setsofttq(void);
extern void setsoftclock(void);

extern void schedsofttty(void);
extern void schedsoftnet(void);
extern void schedsoftcamnet(void);
extern void schedsoftcambio(void);
extern void schedsoftvm(void);
extern void schedsofttq(void);
extern void schedsoftclock(void);

#if 0
/* XXX bogus */
extern		unsigned cpl;	/* current priority level mask */
#endif

#endif /* !_MACHINE_MD_VAR_H_ */
