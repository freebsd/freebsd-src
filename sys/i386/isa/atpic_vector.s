/*-
 * Copyright (c) 1989, 1990 William F. Jolitz.
 * Copyright (c) 1990 The Regents of the University of California.
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
 *	from: vector.s, 386BSD 0.1 unknown origin
 * $FreeBSD$
 */

/*
 * Interrupt entry points for external interrupts triggered by the 8259A
 * master and slave interrupt controllers.
 */

#include "opt_auto_eoi.h"

#include <machine/asmacros.h>
#include <i386/isa/icu.h>
#ifdef PC98
#include <pc98/pc98/pc98.h>
#else
#include <i386/isa/isa.h>
#endif

#include "assym.s"

#define	IRQ_BIT(irq_num)	(1 << ((irq_num) % 8))
#define	IRQ_BYTE(irq_num)	((irq_num) >> 3)

#ifdef AUTO_EOI_1

#define	ENABLE_ICU1		/* use auto-EOI to reduce i/o */
#define	OUTB_ICU1

#else

#define	ENABLE_ICU1							\
	movb	$ICU_EOI,%al ;	/* as soon as possible send EOI ... */	\
	OUTB_ICU1		/* ... to clear in service bit */

#define	OUTB_ICU1							\
	outb	%al,$IO_ICU1

#endif

#ifdef AUTO_EOI_2
/*
 * The data sheet says no auto-EOI on slave, but it sometimes works.
 */
#define	ENABLE_ICU1_AND_2	ENABLE_ICU1

#else

#define	ENABLE_ICU1_AND_2						\
	movb	$ICU_EOI,%al ;	/* as above */				\
	outb	%al,$IO_ICU2 ;	/* but do second icu first ... */	\
	OUTB_ICU1		/* ... then first icu (if !AUTO_EOI_1) */

#endif

/*
 * Macros for interrupt interrupt entry, call to handler, and exit.
 *
 * XXX Most of the parameters here are obsolete.  Fix this when we're
 * done.
 * XXX we really shouldn't return via doreti if we just schedule the
 * interrupt handler and don't run anything.  We could just do an
 * iret.  FIXME.
 */
#define	INTR(irq_num, vec_name, icu, enable_icus, maybe_extra_ipending) \
	.text ;								\
	SUPERALIGN_TEXT ;						\
IDTVEC(vec_name) ;							\
	pushl	$0 ;		/* dummy error code */			\
	pushl	$0 ;		/* dummy trap type */			\
	pushal ;		/* 8 ints */				\
	pushl	%ds ;		/* save data and extra segments ... */	\
	pushl	%es ;							\
	pushl	%fs ;							\
	mov	$KDSEL,%ax ;	/* load kernel ds, es and fs */		\
	mov	%ax,%ds ;						\
	mov	%ax,%es ;						\
	mov	$KPSEL,%ax ;						\
	mov	%ax,%fs ;						\
;									\
	maybe_extra_ipending ;						\
	movb	imen + IRQ_BYTE(irq_num),%al ;				\
	orb	$IRQ_BIT(irq_num),%al ;					\
	movb	%al,imen + IRQ_BYTE(irq_num) ;				\
	outb	%al,$icu+ICU_IMR_OFFSET ;				\
	enable_icus ;							\
;									\
	movl	PCPU(CURTHREAD),%ebx ;					\
	incl	TD_INTR_NESTING_LEVEL(%ebx) ;				\
;									\
	FAKE_MCOUNT(13*4(%esp)) ;	/* XXX late to avoid double count */ \
	pushl	$irq_num; 	/* pass the IRQ */			\
	call	atpic_sched_ithd ;					\
	addl	$4, %esp ;	/* discard the parameter */		\
;									\
	decl	TD_INTR_NESTING_LEVEL(%ebx) ;				\
	MEXITCOUNT ;							\
	jmp	doreti

MCOUNT_LABEL(bintr)
#define	CLKINTR_PENDING	movl $1,CNAME(clkintr_pending)
/* Threaded interrupts */
	INTR(0,atpic_intr0, IO_ICU1, ENABLE_ICU1, CLKINTR_PENDING)
	INTR(1,atpic_intr1, IO_ICU1, ENABLE_ICU1,)
	INTR(2,atpic_intr2, IO_ICU1, ENABLE_ICU1,)
	INTR(3,atpic_intr3, IO_ICU1, ENABLE_ICU1,)
	INTR(4,atpic_intr4, IO_ICU1, ENABLE_ICU1,)
	INTR(5,atpic_intr5, IO_ICU1, ENABLE_ICU1,)
	INTR(6,atpic_intr6, IO_ICU1, ENABLE_ICU1,)
	INTR(7,atpic_intr7, IO_ICU1, ENABLE_ICU1,)
	INTR(8,atpic_intr8, IO_ICU2, ENABLE_ICU1_AND_2,)
	INTR(9,atpic_intr9, IO_ICU2, ENABLE_ICU1_AND_2,)
	INTR(10,atpic_intr10, IO_ICU2, ENABLE_ICU1_AND_2,)
	INTR(11,atpic_intr11, IO_ICU2, ENABLE_ICU1_AND_2,)
	INTR(12,atpic_intr12, IO_ICU2, ENABLE_ICU1_AND_2,)
	INTR(13,atpic_intr13, IO_ICU2, ENABLE_ICU1_AND_2,)
	INTR(14,atpic_intr14, IO_ICU2, ENABLE_ICU1_AND_2,)
	INTR(15,atpic_intr15, IO_ICU2, ENABLE_ICU1_AND_2,)
MCOUNT_LABEL(eintr)
