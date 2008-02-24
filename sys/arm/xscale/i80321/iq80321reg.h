/*	$NetBSD: iq80321reg.h,v 1.4 2003/05/14 19:46:39 thorpej Exp $	*/

/*-
 * Copyright (c) 2002 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Jason R. Thorpe for Wasabi Systems, Inc.
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
 *	This product includes software developed for the NetBSD Project by
 *	Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/arm/xscale/i80321/iq80321reg.h,v 1.3 2006/08/24 23:51:28 cognet Exp $
 *
 */

#ifndef _IQ80321REG_H_
#define	_IQ80321REG_H_

/*
 * Memory map and register definitions for the Intel IQ80321
 * Evaluation Board.
 */

/*
 * The memory map of the IQ80321 looks like so:
 *
 *           ------------------------------
 *		Intel 80321 IOP Reserved
 * FFFF E900 ------------------------------
 *		Peripheral Memory Mapped
 *		    Registers
 * FFFF E000 ------------------------------
 *		On-board devices
 * FE80 0000 ------------------------------
 *		SDRAM
 * A000 0000 ------------------------------
 *		Reserved
 * 9100 0000 ------------------------------
 * 		Flash
 * 9080 0000 ------------------------------
 *		Reserved
 * 9002 0000 ------------------------------
 *		ATU Outbound Transaction
 *		    Windows
 * 8000 0000 ------------------------------
 *		ATU Outbound Direct
 *		    Addressing Windows
 * 0000 1000 ------------------------------
 *		Initialization Boot Code
 *		    from Flash
 * 0000 0000 ------------------------------
 */

/*
 * We allocate a page table for VA 0xfe400000 (4MB) and map the
 * PCI I/O space (64K) and i80321 memory-mapped registers (4K) there.
 */
#define	IQ80321_IOPXS_VBASE	0xfe400000UL
#define	IQ80321_IOW_VBASE	IQ80321_IOPXS_VBASE
#define	IQ80321_80321_VBASE	(IQ80321_IOW_VBASE +			\
				 VERDE_OUT_XLATE_IO_WIN_SIZE)

#define	IQ80321_SDRAM_START	0xa0000000
/*
 * The IQ80321 on-board devices are mapped VA==PA during bootstrap.
 * Conveniently, the size of the on-board register space is 1 section
 * mapping.
 */
#define	IQ80321_OBIO_BASE	0xfe800000UL
#define	IQ80321_OBIO_SIZE	0x00100000UL	/* 1MB */

#define	IQ80321_UART1		0xfe800000UL	/* TI 16550 */

#if defined( CPU_XSCALE_80321 )
#define	IQ80321_7SEG_MSB	0xfe840000UL
#define	IQ80321_7SEG_LSB	0xfe850000UL

#define	IQ80321_ROT_SWITCH	0xfe8d0000UL

#define	IQ80321_BATTERY_STAT	0xfe8f0000UL
#define	BATTERY_STAT_PRES	(1U << 0)
#define	BATTERY_STAT_CHRG	(1U << 1)
#define	BATTERY_STAT_DISCHRG	(1U << 2)
#endif /* CPU_XSCALE_80321 */

#endif /* _IQ80321REG_H_ */
