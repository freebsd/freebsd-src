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
 * $FreeBSD$
 */

/*
 * AT/386 Interrupt Control constants
 * W. Jolitz 8/89
 */

#ifndef _I386_ISA_ICU_H_
#define	_I386_ISA_ICU_H_

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

/* Initialization control word 1. Written to even address. */
#define	ICW1_IC4	0x01		/* ICW4 present */
#define	ICW1_SNGL	0x02		/* 1 = single, 0 = cascaded */
#define	ICW1_ADI	0x04		/* 1 = 4, 0 = 8 byte vectors */
#define	ICW1_LTIM	0x08		/* 1 = level trigger, 0 = edge */
#define	ICW1_RESET	0x10		/* must be 1 */
/* 0x20 - 0x80 - in 8080/8085 mode only */

/* Initialization control word 2. Written to the odd address. */
/* No definitions, it is the base vector of the IDT for 8086 mode */

/* Initialization control word 3. Written to the odd address. */
/* For a master PIC, bitfield indicating a slave 8259 on given input */
/* For slave, lower 3 bits are the slave's ID binary id on master */

/* Initialization control word 4. Written to the odd address. */
#define	ICW4_8086	0x01		/* 1 = 8086, 0 = 8080 */
#define	ICW4_AEOI	0x02		/* 1 = Auto EOI */
#define	ICW4_MS		0x04		/* 1 = buffered master, 0 = slave */
#define	ICW4_BUF	0x08		/* 1 = enable buffer mode */
#define	ICW4_SFNM	0x10		/* 1 = special fully nested mode */

/* Operation control words.  Written after initialization. */

/* Operation control word type 1 */
/*
 * No definitions.  Written to the odd address.  Bitmask for interrupts.
 * 1 = disabled.
 */

/* Operation control word type 2.  Bit 3 (0x08) must be zero. Even address. */
#define	OCW2_L0		0x01		/* Level */
#define	OCW2_L1		0x02
#define	OCW2_L2		0x04
/* 0x08 must be 0 to select OCW2 vs OCW3 */
/* 0x10 must be 0 to select OCW2 vs ICW1 */
#define	OCW2_EOI	0x20		/* 1 = EOI */
#define	OCW2_SL		0x40		/* EOI mode */
#define	OCW2_R		0x80		/* EOI mode */

/* Operation control word type 3.  Bit 3 (0x08) must be set. Even address. */
#define	OCW3_RIS	0x01		/* 1 = read IS, 0 = read IR */
#define	OCW3_RR		0x02		/* register read */
#define	OCW3_P		0x04		/* poll mode command */
/* 0x08 must be 1 to select OCW3 vs OCW2 */
#define	OCW3_SEL	0x08		/* must be 1 */
/* 0x10 must be 0 to select OCW3 vs ICW1 */
#define	OCW3_SMM	0x20		/* special mode mask */
#define	OCW3_ESMM	0x40		/* enable SMM */

/*
 * Interrupt Control offset into Interrupt descriptor table (IDT)
 */
#define	ICU_OFFSET	32		/* 0-31 are processor exceptions */
#define	ICU_LEN		16		/* 32-47 are ISA interrupts */
#define	ICU_IMR_OFFSET	1
#define	ICU_SLAVEID	2
#define	ICU_EOI		(OCW2_EOI)	/* non-specific EOI */

#ifndef LOCORE
void	atpic_handle_intr(void *cookie, struct intrframe iframe);
void	atpic_startup(void);
#endif

#endif /* !_I386_ISA_ICU_H_ */
