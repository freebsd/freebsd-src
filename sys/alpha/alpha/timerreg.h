/*-
 * Copyright (c) 1993 The Regents of the University of California.
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
 *      This product includes software developed by the University of
 *      California, Berkeley and its contributors.
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
 *	from: Header: timerreg.h,v 1.2 93/02/28 15:08:58 mccanne Exp
 * $FreeBSD: src/sys/alpha/alpha/timerreg.h,v 1.2.2.1 2000/07/03 19:59:44 mjacob Exp $
 */

/*
 *
 * Register definitions for the Intel 8253 Programmable Interval Timer.
 *
 * This chip has three independent 16-bit down counters that can be
 * read on the fly.  There are three mode registers and three countdown
 * registers.  The countdown registers are addressed directly, via the
 * first three I/O ports.  The three mode registers are accessed via
 * the fourth I/O port, with two bits in the mode byte indicating the
 * register.  (Why are hardware interfaces always so braindead?).
 *
 * To write a value into the countdown register, the mode register
 * is first programmed with a command indicating the which byte of
 * the two byte register is to be modified.  The three possibilities
 * are load msb (TMR_MR_MSB), load lsb (TMR_MR_LSB), or load lsb then
 * msb (TMR_MR_BOTH).
 *
 * To read the current value ("on the fly") from the countdown register,
 * you write a "latch" command into the mode register, then read the stable
 * value from the corresponding I/O port.  For example, you write
 * TMR_MR_LATCH into the corresponding mode register.  Presumably,
 * after doing this, a write operation to the I/O port would result
 * in undefined behavior (but hopefully not fry the chip).
 * Reading in this manner has no side effects.
 *
 * [IBM-PC]
 * The outputs of the three timers are connected as follows:
 *
 *	 timer 0 -> irq 0
 *	 timer 1 -> dma chan 0 (for dram refresh)
 * 	 timer 2 -> speaker (via keyboard controller)
 *
 * Timer 0 is used to call hardclock.
 * Timer 2 is used to generate console beeps.
 *
 * [PC-9801]
 * The outputs of the three timers are connected as follows:
 *
 *	 timer 0 -> irq 0
 *	 timer 1 -> speaker (via keyboard controller)
 * 	 timer 2 -> RS232C
 *
 * Timer 0 is used to call hardclock.
 * Timer 1 is used to generate console beeps.
 */

/*
 * Macros for specifying values to be written into a mode register.
 */
#define	TIMER_CNTR0	(IO_TIMER1 + 0)	/* timer 0 counter port */
#ifdef PC98
#define	TIMER_CNTR1	0x3fdb		/* timer 1 counter port */
#define	TIMER_CNTR2	(IO_TIMER1 + 4)	/* timer 2 counter port */
#define	TIMER_MODE	(IO_TIMER1 + 6)	/* timer mode port */
#else
#define	TIMER_CNTR1	(IO_TIMER1 + 1)	/* timer 1 counter port */
#define	TIMER_CNTR2	(IO_TIMER1 + 2)	/* timer 2 counter port */
#define	TIMER_MODE	(IO_TIMER1 + 3)	/* timer mode port */
#endif
#define		TIMER_SEL0	0x00	/* select counter 0 */
#define		TIMER_SEL1	0x40	/* select counter 1 */
#define		TIMER_SEL2	0x80	/* select counter 2 */
#define		TIMER_INTTC	0x00	/* mode 0, intr on terminal cnt */
#define		TIMER_ONESHOT	0x02	/* mode 1, one shot */
#define		TIMER_RATEGEN	0x04	/* mode 2, rate generator */
#define		TIMER_SQWAVE	0x06	/* mode 3, square wave */
#define		TIMER_SWSTROBE	0x08	/* mode 4, s/w triggered strobe */
#define		TIMER_HWSTROBE	0x0a	/* mode 5, h/w triggered strobe */
#define		TIMER_LATCH	0x00	/* latch counter for reading */
#define		TIMER_LSB	0x10	/* r/w counter LSB */
#define		TIMER_MSB	0x20	/* r/w counter MSB */
#define		TIMER_16BIT	0x30	/* r/w counter 16 bits, LSB first */
#define		TIMER_BCD	0x01	/* count in BCD */

