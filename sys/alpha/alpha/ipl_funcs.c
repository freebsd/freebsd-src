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
 *	$Id: ipl_funcs.c,v 1.1 1998/06/10 10:52:49 dfr Exp $
 */

#include <sys/types.h>
#include <sys/systm.h>
#include <machine/ipl.h>
#include <machine/cpu.h>
#include <net/netisr.h>

unsigned int netisr;
void	(*netisrs[32]) __P((void));
u_int64_t ipending;
int cpl;

static void atomic_setbit(u_int64_t* p, u_int64_t bit)
{
    u_int64_t temp;
    __asm__ __volatile__ (
	"1:\tldq_l %0,%2\n\t"	/* load current mask value, asserting lock */
	"or %3,%0,%0\n\t"	/* add our bits */
	"stq_c %0,%1\n\t"	/* attempt to store */
	"beq %0,2f\n\t"		/* if the store failed, spin */
	"br 3f\n"		/* it worked, exit */
	"2:\tbr 1b\n"		/* *p not updated, loop */
	"3:\tmb\n"		/* it worked */
	: "=&r"(temp), "=m" (*p)
	: "m"(*p), "r"(bit)
	: "memory");
}

static u_int64_t atomic_readandclear(u_int64_t* p)
{
    u_int64_t v, temp;
    __asm__ __volatile__ (
	"wmb\n"			/* ensure pending writes have drained */
	"1:\tldq_l %0,%3\n\t"	/* load current value, asserting lock */
	"ldiq %1,0\n\t"		/* value to store */
	"stq_c %1,%2\n\t"	/* attempt to store */
	"beq %1,2f\n\t"		/* if the store failed, spin */
	"br 3f\n"		/* it worked, exit */
	"2:\tbr 1b\n"		/* *p not updated, loop */
	"3:\tmb\n"		/* it worked */
	: "=&r"(v), "=&r"(temp), "=m" (*p)
	: "m"(*p)
	: "memory");
    return v;
}

void
do_sir()
{
    u_int64_t pend = atomic_readandclear(&ipending);
#if 0
    /*
     * Later - no users of these yet.
     */ 
    if (pend & (1 << SWI_TTY))
	swi_tty();
    if (pend & (1 << SWI_NET))
	swi_net();
#endif
    if (pend & (1 << SWI_CLOCK))
	softclock();
}


#define GENSETSOFT(name, bit)			\
						\
void name(void)					\
{						\
    atomic_setbit(&ipending, (1 << bit));	\
}

GENSETSOFT(setsofttty, SWI_TTY)
GENSETSOFT(setsoftnet, SWI_NET)
GENSETSOFT(setsoftcamnet, SWI_CAMNET)
GENSETSOFT(setsoftcambio, SWI_CAMBIO)
GENSETSOFT(setsoftvm, SWI_VM)
GENSETSOFT(setsoftclock, SWI_CLOCK)

#define SPLDOWN(name, pri)				\
							\
int name(void)						\
{							\
    int s = alpha_pal_swpipl(ALPHA_PSL_IPL_##pri);	\
    cpl = ALPHA_PSL_IPL_##pri;				\
    return s;						\
}

SPLDOWN(splsoftclock, SOFT)
SPLDOWN(splsoftnet, SOFT)

#define SPLUP(name, pri)				\
							\
int name(void)						\
{							\
    if (ALPHA_PSL_IPL_##pri > cpl) {			\
	int s = alpha_pal_swpipl(ALPHA_PSL_IPL_##pri);	\
	cpl = ALPHA_PSL_IPL_##pri;			\
	return s;					\
    } else						\
	return cpl;					\
}

SPLUP(splnet, IO)
SPLUP(splbio, IO)
SPLUP(splimp, IO)
SPLUP(spltty, IO)
SPLUP(splvm, IO)
SPLUP(splclock, CLOCK)
SPLUP(splstatclock, CLOCK)
SPLUP(splhigh, HIGH)

void
spl0()
{
    /* XXX soft interrupts here */

    alpha_pal_swpipl(ALPHA_PSL_IPL_0);
    cpl = ALPHA_PSL_IPL_0;
}

void
splx(int s)
{
    if (s) {
	alpha_pal_swpipl(s);
	cpl = s;
    } else
	spl0();
}

