/*	$NetBSD: sa11x0_irqhandler.c,v 1.5 2003/08/07 16:26:54 agc Exp $	*/

/*-
 * Copyright (c) 1996, 1997, 1998, 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to the NetBSD Foundation
 * by IWAMOTO Toshihiro.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum and by Jason R. Thorpe of the Numerical Aerospace
 * Simulation Facility, NASA Ames Research Center.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
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
 */

/*-
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)isa.c	7.2 (Berkeley) 5/13/91
 */


#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/syslog.h>
#include <sys/malloc.h>
#include <sys/types.h>
#include <sys/bus.h>
#include <sys/interrupt.h>
#include <vm/vm.h>
#include <vm/vm_extern.h>

#include <arm/sa11x0/sa11x0_reg.h>
#include <arm/sa11x0/sa11x0_var.h>

#include <machine/cpu.h>

#define NIRQS 0x20
struct intrhand *irqhandlers[NIRQS];

int current_intr_depth;
u_int actual_mask;
#define IPL_LEVELS 13
#ifdef hpcarm
#define IPL_LEVELS (NIPL+1)
u_int imask[NIPL];
#else
u_int spl_mask;
u_int irqmasks[IPL_LEVELS];
#endif
u_int irqblock[NIRQS];
u_int levels[IPL_LEVELS];


extern void set_spl_masks(void);
#if 0
static int fakeintr(void *);
#endif
#ifdef DEBUG
static int dumpirqhandlers(void);
#endif

/* Recalculate the interrupt masks from scratch.
 * We could code special registry and deregistry versions of this function that
 * would be faster, but the code would be nastier, and we don't expect this to
 * happen very much anyway.
 */
void intr_calculatemasks(void);
void
intr_calculatemasks(void)
{       
	int irq;
	int intrlevel[ICU_LEN];
	int level;

	/* First, figure out which levels each IRQ uses. */
	for (irq = 0; irq < ICU_LEN; irq++) {
			intrlevel[irq] = levels[irq];
	}
	/* Then figure out which IRQs use each level. */        
#ifdef hpcarm
	for (level = 0; level < NIPL; level++) {
#else
	for (level = 0; level <= IPL_LEVELS; level++) {
#endif
		int irqs = 0;
		for (irq = 0; irq < ICU_LEN; irq++) {
			if (intrlevel[irq] & (1 << level)) {
				irqs |= 1 << irq;
			}
		}
#ifdef hpcarm

		imask[level] = irqs;
#else
		irqmasks[level] = irqs;
		printf("level %d set to %x\n", level, irqs);
#endif
	}
	        /*
       		 * Enforce a hierarchy that gives slow devices a better chance at not
		 * dropping data.                                     
		 */
#ifdef hpcarm
	for (level = NIPL - 1; level > 0; level--)
		imask[level - 1] |= imask[level];
#else
	for (level = IPL_LEVELS; level > 0; level--)
		irqmasks[level - 1] |= irqmasks[level];
#endif
        /*
	 * Calculate irqblock[], which emulates hardware interrupt levels.
	 */
#if 0
	for (irq = 0; irq < ICU_LEN; irq++) {
		int irqs = 1 << irq;
		for (q = irqhandlers[irq]; q; q = q->ih_next)
#ifdef hpcarm           
			irqs |= ~imask[q->ih_level];
#else                   
			irqs |= ~irqmasks[q->ih_level];       
#endif
		irqblock[irq] = irqs;
	}
#endif
}
					
const struct evcnt *sa11x0_intr_evcnt(sa11x0_chipset_tag_t, int);
void stray_irqhandler(void *);


const struct evcnt *
sa11x0_intr_evcnt(sa11x0_chipset_tag_t ic, int irq)
{

	/* XXX for now, no evcnt parent reported */
	return NULL;
}


void
stray_irqhandler(void *p)
{

	printf("stray interrupt %p\n", p);
	printf("bla\n");
}

#if 0
int
fakeintr(void *p)
{

	return 0;
}
#endif
#ifdef DEBUG
int
dumpirqhandlers()
{
	int irq;
	struct irqhandler *p;

	for (irq = 0; irq < ICU_LEN; irq++) {
		printf("irq %d:", irq);
		p = irqhandlers[irq];
		for (; p; p = p->ih_next)
			printf("ih_func: 0x%lx, ", (unsigned long)p->ih_func);
		printf("\n");
	}
	return 0;
}
#endif
/* End of irqhandler.c */
