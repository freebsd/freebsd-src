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
 *	$Id: icu.h,v 1.13 1997/05/29 04:58:04 peter Exp $
 */

/*
 * AT/386 Interrupt Control constants
 * W. Jolitz 8/89
 */

#ifndef _I386_ISA_ICU_H_
#define	_I386_ISA_ICU_H_

#ifndef	LOCORE

/*
 * Interrupt "level" mechanism variables, masks, and macros
 */
extern	unsigned imen;		/* interrupt mask enable */

#if defined(APIC_IO)

# if !defined(_MACHINE_SMP_H_)
/** XXX what a hack, its this or include <machine/smp.h>! */
void write_io_apic_mask24 __P((int, u_int32_t)); /* i386/i386/mpapic.c */
# endif /* _MACHINE_SMP_H_ */

#if defined(MULTIPLE_IOAPICS)
#error MULTIPLE_IOAPICSXXX: cannot assume apic #0 in the following functions.
#endif /* MULTIPLE_IOAPICS */

#ifdef nevermore
/* see if we can get by without these, they're NOT MP_SAFE... */
static __inline u_int32_t
INTRGET(void)
{
	return (imen & 0x00ffffff);		/* return our global copy */
}

static __inline void
INTRSET(unsigned s)
{
	write_io_apic_mask24(0, s);
	imen = s;
}
#endif /** nevermore */

static __inline void
INTREN(unsigned s)
{
	write_io_apic_mask24(0, imen & ~s);
	imen &= ~s;
}

static __inline void
INTRDIS(unsigned s)
{
	write_io_apic_mask24(0, imen | s);
	imen |= s;
}


#define	INTRMASK(msk,s)		(msk |= (s))
#define INTRUNMASK(msk,s)	(msk &= ~(s))

#else /* APIC_IO */

#define	INTREN(s)		(imen &= ~(s), SET_ICUS())
#define	INTRDIS(s)		(imen |= (s), SET_ICUS())
#define	INTRMASK(msk,s)		(msk |= (s))
#define INTRUNMASK(msk,s)	(msk &= ~(s))

#if 0
#ifdef PC98
#define	SET_ICUS()	(outb(IO_ICU1 + 2, imen), outb(IU_ICU2 + 2, imen >> 8))
#define INTRGET()	((inb(IO_ICU2) << 8 | inb(IO_ICU1)) & 0xffff)
#else	/* IBM-PC */
#define	SET_ICUS()	(outb(IO_ICU1 + 1, imen), outb(IU_ICU2 + 1, imen >> 8))
#define INTRGET()	((inb(IO_ICU2) << 8 | inb(IO_ICU1)) & 0xffff)
#endif	/* PC98 */
#else
/*
 * XXX - IO_ICU* are defined in isa.h, not icu.h, and nothing much bothers to
 * include isa.h, while too many things include icu.h.
 */
#ifdef PC98
#define	SET_ICUS()	(outb(0x02, imen), outb(0x0a, imen >> 8))
/* XXX is this correct? */
#define INTRGET()	((inb(0x0a) << 8 | inb(0x02)) & 0xffff)
#else
#define	SET_ICUS()	(outb(0x21, imen), outb(0xa1, imen >> 8))
#define INTRGET()	((inb(0xa1) << 8 | inb(0x21)) & 0xffff)
#endif
#endif

#endif /* APIC_IO */

#endif /* LOCORE */

/*
 * Interrupt enable bits - in normal order of priority (which we change)
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
#define	IRQ3		0x0008		/* this is highest after rotation */
#define	IRQ4		0x0010
#define	IRQ5		0x0020
#define	IRQ6		0x0040
#define	IRQ7		0x0080		/* lowest - parallel printer */

#ifdef PC98
#undef	IRQ2
#define IRQ2		0x0004
#undef	IRQ_SLAVE
#define	IRQ_SLAVE	0x0080
#endif

/*
 * Interrupt Control offset into Interrupt descriptor table (IDT)
 */
#define	ICU_OFFSET	32		/* 0-31 are processor exceptions */

#if defined(APIC_IO)

/* 32-47: ISA IRQ0-IRQ15, 48-55: IO APIC IRQ16-IRQ23 */
#define	ICU_LEN		24

#else

#define	ICU_LEN		16		/* 32-47 are ISA interrupts */

#endif /* APIC_IO */

#endif /* !_I386_ISA_ICU_H_ */
