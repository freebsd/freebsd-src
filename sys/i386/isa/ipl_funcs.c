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
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <machine/ipl.h>
#include <sys/proc.h>
#include <i386/isa/icu.h>
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
	sched_ithd((void *) SOFTINTR);		\
}

DO_SETBITS(setdelayed,   &spending, loadandclear(&idelayed))
DO_SETBITS(setsoftcamnet,&spending, SWI_CAMNET_PENDING)
DO_SETBITS(setsoftcambio,&spending, SWI_CAMBIO_PENDING)
DO_SETBITS(setsoftclock, &spending, SWI_CLOCK_PENDING)
DO_SETBITS(setsoftnet,   &spending, SWI_NET_PENDING)
DO_SETBITS(setsofttty,   &spending, SWI_TTY_PENDING)
DO_SETBITS(setsoftvm,	 &spending, SWI_VM_PENDING)
DO_SETBITS(setsofttq,	 &spending, SWI_TQ_PENDING)

/*
 * We don't need to schedule soft interrupts any more, it happens
 * automatically.
 */ 
#define	schedsoftcamnet
#define	schedsoftcambio
#define	schedsoftnet
#define	schedsofttty
#define	schedsoftvm
#define	schedsofttq

unsigned
softclockpending(void)
{
	return (spending & SWI_CLOCK_PENDING);
}

/*
 * Dummy spl calls.  The only reason for these is to not break
 * all the code which expects to call them.
 */
void spl0 (void) {}
void splx (intrmask_t x) {}
intrmask_t  splq(intrmask_t mask) {return 0; }
intrmask_t  splbio(void) {return 0; }
intrmask_t  splcam(void) {return 0; }
intrmask_t  splclock(void) {return 0; }
intrmask_t  splhigh(void) {return 0; }
intrmask_t  splimp(void) {return 0; }
intrmask_t  splnet(void) {return 0; }
intrmask_t  splsoftcam(void) {return 0; }
intrmask_t  splsoftcambio(void) {return 0; }
intrmask_t  splsoftcamnet(void) {return 0; }
intrmask_t  splsoftclock(void) {return 0; }
intrmask_t  splsofttty(void) {return 0; }
intrmask_t  splsoftvm(void) {return 0; }
intrmask_t  splsofttq(void) {return 0; }
intrmask_t  splstatclock(void) {return 0; }
intrmask_t  spltty(void) {return 0; }
intrmask_t  splvm(void) {return 0; }
