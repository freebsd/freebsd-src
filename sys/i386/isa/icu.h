/*-
 * Copyright (c) 1990 The Regents of the University of California.
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
 *	from: @(#)icu.h	5.6 (Berkeley) 5/9/91
 *	$Id: icu.h,v 1.2 1993/10/16 13:45:51 rgrimes Exp $
 */

/*
 * AT/386 Interrupt Control constants
 * W. Jolitz 8/89
 */

#ifndef	__ICU__
#define	__ICU__

#ifndef	LOCORE

/*
 * Interrupt "level" mechanism variables, masks, and macros
 */
extern	unsigned imen;		/* interrupt mask enable */
extern	unsigned cpl;		/* current priority level mask */

extern	unsigned highmask;	/* group of interrupts masked with splhigh() */
extern	unsigned ttymask;	/* group of interrupts masked with spltty() */
extern	unsigned biomask;	/* group of interrupts masked with splbio() */
extern	unsigned netmask;	/* group of interrupts masked with splimp() */

#define	INTREN(s)	(imen &= ~(s), SET_ICUS())
#define	INTRDIS(s)	(imen |= (s), SET_ICUS())
#define	INTRMASK(msk,s)	(msk |= (s))
#if 0
#define SET_ICUS()	(outb(IO_ICU1 + 1, imen), outb(IU_ICU2 + 1, imen >> 8))
#else
/*
 * XXX - IO_ICU* are defined in isa.h, not icu.h, and nothing much bothers to
 * include isa.h, while too many things include icu.h.
 */
#define SET_ICUS()	(outb(0x21, imen), outb(0xa1, imen >> 8))
#endif

#endif

/*
 * Interrupt enable bits -- in order of priority
 */
#define	IRQ0		0x0001		/* highest priority - timer */
#define	IRQ1		0x0002
#define	IRQ_SLAVE	0x0004
#define	IRQ8		0x0100
#define	IRQ9		0x0200
#define	IRQ2		IRQ9
#define	IRQ10		0x0400
#define	IRQ11		0x0800
#define	IRQ12		0x1000
#define	IRQ13		0x2000
#define	IRQ14		0x4000
#define	IRQ15		0x8000
#define	IRQ3		0x0008
#define	IRQ4		0x0010
#define	IRQ5		0x0020
#define	IRQ6		0x0040
#define	IRQ7		0x0080		/* lowest - parallel printer */

/*
 * Interrupt Control offset into Interrupt descriptor table (IDT)
 */
#define	ICU_OFFSET	32		/* 0-31 are processor exceptions */
#define	ICU_LEN		16		/* 32-47 are ISA interrupts */

#endif	__ICU__
