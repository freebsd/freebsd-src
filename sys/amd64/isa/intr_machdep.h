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
 * $FreeBSD$
 */

#ifndef _I386_ISA_INTR_MACHDEP_H_
#define	_I386_ISA_INTR_MACHDEP_H_

/*
 * Low level interrupt code.
 */ 

#ifdef _KERNEL

#ifdef LOCORE

/*
 * Protects the IO APIC, 8259 PIC, imen, and apic_imen
 */
#define ICU_LOCK	MTX_LOCK_SPIN(icu_lock, 0)
#define ICU_UNLOCK	MTX_UNLOCK_SPIN(icu_lock)

#else /* LOCORE */

/*
 * Type of the first (asm) part of an interrupt handler.
 */
typedef void inthand_t(u_int cs, u_int ef, u_int esp, u_int ss);
typedef void unpendhand_t(void);

#define	IDTVEC(name)	__CONCAT(X,name)

extern u_long *intr_countp[];	/* pointers into intrcnt[] */
extern driver_intr_t *intr_handler[];	/* C entry points of intr handlers */
extern struct ithd *ithds[];
extern void *intr_unit[];	/* cookies to pass to intr handlers */
extern struct mtx icu_lock;

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
unpendhand_t
	IDTVEC(fastunpend0), IDTVEC(fastunpend1), IDTVEC(fastunpend2),
	IDTVEC(fastunpend3), IDTVEC(fastunpend4), IDTVEC(fastunpend5),
	IDTVEC(fastunpend6), IDTVEC(fastunpend7), IDTVEC(fastunpend8),
	IDTVEC(fastunpend9), IDTVEC(fastunpend10), IDTVEC(fastunpend11),
	IDTVEC(fastunpend12), IDTVEC(fastunpend13), IDTVEC(fastunpend14),
	IDTVEC(fastunpend15), IDTVEC(fastunpend16), IDTVEC(fastunpend17),
	IDTVEC(fastunpend18), IDTVEC(fastunpend19), IDTVEC(fastunpend20),
	IDTVEC(fastunpend21), IDTVEC(fastunpend22), IDTVEC(fastunpend23),
	IDTVEC(fastunpend24), IDTVEC(fastunpend25), IDTVEC(fastunpend26),
	IDTVEC(fastunpend27), IDTVEC(fastunpend28), IDTVEC(fastunpend29),
	IDTVEC(fastunpend30), IDTVEC(fastunpend31);

#define	NR_INTRNAMES	(1 + ICU_LEN + 2 * ICU_LEN)

void	isa_defaultirq(void);
int	isa_nmi(int cd);
int	icu_setup(int intr, driver_intr_t *func, void *arg, int flags);
int	icu_unset(int intr, driver_intr_t *handler);
void	icu_reinit(void);

/*
 * WARNING: These are internal functions and not to be used by device drivers!
 * They are subject to change without notice. 
 */
int	inthand_add(const char *name, int irq, driver_intr_t handler, void *arg,
	    enum intr_type flags, void **cookiep);
int	inthand_remove(void *cookie);
void	sched_ithd(void *dummy);
void	call_fast_unpend(int irq);

#endif /* LOCORE */

#endif /* _KERNEL */

#endif /* !_I386_ISA_INTR_MACHDEP_H_ */
