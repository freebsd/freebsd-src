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
 *	from: @(#)ns16550.h	7.1 (Berkeley) 5/9/91
 *	$Id: ns16550.h,v 1.5 1997/02/22 09:38:05 peter Exp $
 */

/*
 * NS16550 UART registers
 */
#define	com_data	0	/* data register (R/W) */
#define	com_dlbl	0	/* divisor latch low (W) */
#define	com_dlbh	1	/* divisor latch high (W) */
#define	com_ier		1	/* interrupt enable (W) */
#define	com_iir		2	/* interrupt identification (R) */
#define	com_fifo	2	/* FIFO control (W) */
#define	com_lctl	3	/* line control register (R/W) */
#define	com_cfcr	3	/* line control register (R/W) */
#define	com_mcr		4	/* modem control register (R/W) */
#define	com_lsr		5	/* line status register (R/W) */
#define	com_msr		6	/* modem status register (R/W) */

#ifdef PC98
#define	com_emr		com_msr	/* Extension mode register for RSB-2000/3000. */

/* I/O-DATA RSA Serise Exrension Register */
#define rsa_msr		0	/* Mode Status Register (R/W) */
#define	rsa_ier		1	/* Interrupt Enable Register (R/W) */
#define	rsa_srr		2	/* Status Read Register (R) */
#define	rsa_frr		2	/* FIFO Reset Register (W) */
#define	rsa_tivsr	3	/* Timer Interval Value Set Register (R/W) */
#define	rsa_tcr		4	/* Timer Control Register (W) */

/*
 * RSA-98III RSA Mode Driver Data Sheet
 *
 * <<Register Map>>
 * Base + 0x00
 * Mode Select Register(Read/Write)
 * bit4=interrupt type(1: level, 0: edge)
 * bit3=Auto RTS-CTS Flow Control Enable
 * bit2=External FIFO Enable
 * bit1=Reserved(Default 0)Don't Change!!
 * bit0=Swap Upper 8byte and Lower 8byte in 16byte space.
 *
 * Base + 0x01
 * Interrupt Enable Register(Read/Write)
 * bit4=Hardware Timer Interrupt Enable
 * bit3=Character Time-Out Interrupt Enable
 * bit2=Tx FIFO Empty Interrupt Enable
 * bit1=Tx FIFO Half Full Interrupt Enable
 * bit0=Rx FIFO Half Full Interrupt Enable
 *
 * Base + 0x02
 * Status Read Register(Read)
 * bit7=Hardware  Time Out Interrupt Status(1: True, 0: False)
 * bit6=Character Time Out Interrupt Status
 * bit5=Rx FIFO Full Flag(0: True, 1: False)
 * bit4=Rx FIFO Half Full Flag
 * bit3=Rx FIFO Empty Flag
 * bit2=Tx FIFO Full Flag
 * bit1=Tx FIFO Half Full Flag
 * bit0=Tx FIFO Empty Flag
 *
 * Base + 0x02
 * FIFO Reset Register(Write)
 * Reset Extrnal FIFO
 *
 * Base + 0x03
 * Timer Interval Value Set Register(Read/Write)
 * Range of n: 1-255
 * Interval Value: n * 0.2ms
 *
 * Base + 0x04
 * Timer Control Register(Read/Write)
 * bit0=Timer Enable
 *
 * Base + 0x08 - 0x0f
 * Same as UART 16550
 *
 * Special Regisgter in RSA Mode
 * UART Data Register(Base + 0x08)
 * Data transfer between Extrnal FIFO
 *
 * UART MCR(Base + 0x0c)
 * bit3(OUT2[MCR_IENABLE])=1: Diable 16550   to Rx FIFO transfer
 * bit2(OUT1[MCR_DRS])=1:     Diable Tx FIFO to 16550   transfer
 *
 * <<Intrrupt and Intrrupt Reset>>
 * o Reciver Line Status(from UART16550)
 *   Reset: Read LSR
 *
 * o Modem Status(from UART16550)
 *   Reset: Read MSR
 *
 * o Rx FIFO Half Full(from Extrnal FIFO)
 *   Reset: Read Rx FIFO under Hall Full 
 *
 * o Character Time Out(from Extrnal FIFO)
 *   Reset: Read Rx FIFO or SRR
 *
 * o Tx FIFO Empty(from Extrnal FIFO)
 *   Reset: Write Tx FIFO or Read SRR
 *
 * o Tx FIFO Half Full(from Extrnal FIFO)
 *   Reset: Write Tx FIFO until Hall Full or Read SRR
 * 
 * o Hardware Timer(from Extrnal FIFO)
 *   Reset: Disable Timer in TCR
 *   Notes: If you want to use Timer for next intrrupt,
 *          you must enable Timer in TCR
 *
 * <<Used Setting>>
 * Auto RTS-CTS:    Enable or Disable
 * External FIFO:   Enable
 * Swap 8bytes:     Disable
 * Haredware Timer: Disable
 * interrupt type:  edge
 * interrupt source:
 *           Hareware Timer
 *           Character Time Out
 *           Tx FIFO Empty
 *           Rx FIFO Half Full
 *
 */
#endif /* PC98 */
