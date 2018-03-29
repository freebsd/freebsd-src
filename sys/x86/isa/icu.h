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

#ifndef _X86_ISA_ICU_H_
#define	_X86_ISA_ICU_H_

#ifdef PC98
#define	ICU_IMR_OFFSET	2
#else
#define	ICU_IMR_OFFSET	1
#endif

/*
 * PC-98 machines wire the slave 8259A to pin 7 on the master PIC, and
 * PC-AT machines wire the slave PIC to pin 2 on the master PIC.
 */
#ifdef PC98
#define	ICU_SLAVEID	7
#else
#define	ICU_SLAVEID	2
#endif

/*
 * Determine the base master and slave modes not including auto EOI support.
 * All machines that FreeBSD supports use 8086 mode.
 */
#ifdef PC98
/*
 * PC-98 machines do not support auto EOI on the second PIC.  Also, it
 * seems that PC-98 machine PICs use buffered mode, and the master PIC
 * uses special fully nested mode.
 */
#define	BASE_MASTER_MODE	(ICW4_SFNM | ICW4_BUF | ICW4_MS | ICW4_8086)
#define	BASE_SLAVE_MODE		(ICW4_BUF | ICW4_8086)
#else
#define	BASE_MASTER_MODE	ICW4_8086
#define	BASE_SLAVE_MODE		ICW4_8086
#endif

/* Enable automatic EOI if requested. */
#ifdef AUTO_EOI_1
#define	MASTER_MODE		(BASE_MASTER_MODE | ICW4_AEOI)
#else
#define	MASTER_MODE		BASE_MASTER_MODE
#endif
#ifdef AUTO_EOI_2
#define	SLAVE_MODE		(BASE_SLAVE_MODE | ICW4_AEOI)
#else
#define	SLAVE_MODE		BASE_SLAVE_MODE
#endif

#define	IRQ_MASK(irq)		(1 << (irq))
#define	IMEN_MASK(ai)		(IRQ_MASK((ai)->at_irq))

void	atpic_handle_intr(u_int vector, struct trapframe *frame);
void	atpic_startup(void);

#endif /* !_X86_ISA_ICU_H_ */
