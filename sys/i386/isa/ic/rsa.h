/*-
 * Copyright (c) 1999 FreeBSD Inc.
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/i386/isa/ic/rsa.h,v 1.2 1999/08/28 00:45:15 peter Exp $
 */

/*
 * RSA Mode Driver Data Sheet
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

/* I/O-DATA RSA Serise Exrension Register */
#define rsa_msr		0	/* Mode Status Register (R/W) */
#define	rsa_ier		1	/* Interrupt Enable Register (R/W) */
#define	rsa_srr		2	/* Status Read Register (R) */
#define	rsa_frr		2	/* FIFO Reset Register (W) */
#define	rsa_tivsr	3	/* Timer Interval Value Set Register (R/W) */
#define	rsa_tcr		4	/* Timer Control Register (W) */
