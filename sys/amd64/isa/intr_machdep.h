/*-
 * Copyright (c) 1991 The Regents of the University of California.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 *	from: @(#)isa_device.h	7.1 (Berkeley) 5/9/91
 *	$Id: intr_machdep.h,v 1.2 1997/06/27 23:48:05 fsmp Exp $
 */

#ifndef _I386_ISA_INTR_MACHDEP_H_
#define	_I386_ISA_INTR_MACHDEP_H_

/*
 * Low level interrupt code.
 */ 

#ifdef KERNEL

/*
 * Type of the first (asm) part of an interrupt handler.
 */
typedef void inthand_t __P((u_int cs, u_int ef, u_int esp, u_int ss));

#define	IDTVEC(name)	__CONCAT(X,name)

extern char eintrnames[];	/* end of intrnames[] */
extern u_long intrcnt[];	/* counts for for each device and stray */
extern char intrnames[];	/* string table containing device names */
extern u_long *intr_countp[];	/* pointers into intrcnt[] */
extern inthand2_t *intr_handler[];	/* C entry points of intr handlers */
extern u_int intr_mask[];	/* sets of intrs masked during handling of 1 */
extern int intr_unit[];		/* cookies to pass to intr handlers */

inthand_t
	IDTVEC(fastintr0), IDTVEC(fastintr1),
	IDTVEC(fastintr2), IDTVEC(fastintr3),
	IDTVEC(fastintr4), IDTVEC(fastintr5),
	IDTVEC(fastintr6), IDTVEC(fastintr7),
	IDTVEC(fastintr8), IDTVEC(fastintr9),
	IDTVEC(fastintr10), IDTVEC(fastintr11),
	IDTVEC(fastintr12), IDTVEC(fastintr13),
	IDTVEC(fastintr14), IDTVEC(fastintr15);
inthand_t
	IDTVEC(intr0), IDTVEC(intr1), IDTVEC(intr2), IDTVEC(intr3),
	IDTVEC(intr4), IDTVEC(intr5), IDTVEC(intr6), IDTVEC(intr7),
	IDTVEC(intr8), IDTVEC(intr9), IDTVEC(intr10), IDTVEC(intr11),
	IDTVEC(intr12), IDTVEC(intr13), IDTVEC(intr14), IDTVEC(intr15);

/* these functions ONLY exist in an SMP/APIC_IO kernel: */
inthand_t
	IDTVEC(fastintr16), IDTVEC(fastintr17),
	IDTVEC(fastintr18), IDTVEC(fastintr19),
	IDTVEC(fastintr20), IDTVEC(fastintr21),
	IDTVEC(fastintr22), IDTVEC(fastintr23);
inthand_t
	IDTVEC(intr16), IDTVEC(intr17), IDTVEC(intr18), IDTVEC(intr19),
	IDTVEC(intr20), IDTVEC(intr21), IDTVEC(intr22), IDTVEC(intr23);

#define XINVLTLB_OFFSET	(ICU_OFFSET + 32)
inthand_t
	Xinvltlb;

#if defined(TEST_CPUSTOP)
#define XCPUSTOP_OFFSET	(ICU_OFFSET + 64)
inthand_t
	Xcpustop;
/*
 * XXX FIXME: rethink location for this (and all IPI vectors)
 * Note: this vector MUST be xxxx1111, 32 + 79 = 111 = 0x6f:
 * also remember i386/include/segments.h: #define	NIDT	129
 */
#define XSPURIOUSINT_OFFSET	(ICU_OFFSET + 79)
inthand_t
	Xspuriousint;
#endif  /* TEST_CPUSTOP */

struct isa_device;

void	isa_defaultirq __P((void));
int	isa_irq_pending __P((struct isa_device *dvp));
/* this function ONLY exists in an SMP/APIC_IO kernel: */
int	icu_irq_pending __P((struct isa_device *dvp));
int	isa_nmi __P((int cd));
void	update_intrname __P((int intr, int device_id));
int	icu_setup __P((int intr, inthand2_t *func, void *arg, 
		       u_int *maskptr, int flags));
int	icu_unset __P((int intr, inthand2_t *handler));
int	update_intr_masks __P((void));
void	register_imask __P((struct isa_device *dvp, u_int mask));

#endif /* KERNEL */

#endif /* !_I386_ISA_INTR_MACHDEP_H_ */
