/*	$NetBSD: adm5120reg.h,v 1.1 2007/03/20 08:52:03 dyoung Exp $	*/

/*-
 * Copyright (c) 2007 Ruslan Ermilov and Vsevolod Lobko.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or
 * without modification, are permitted provided that the following
 * conditions are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 * 3. The names of the authors may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _ADM5120REG_H_
#define _ADM5120REG_H_

/* Helpers from NetBSD */
/* __BIT(n): nth bit, where __BIT(0) == 0x1. */
#define __BIT(__n)      \
        (((__n) >= NBBY * sizeof(uintmax_t)) ? 0 : ((uintmax_t)1 << (__n)))

/* __BITS(m, n): bits m through n, m < n. */
#define __BITS(__m, __n)        \
        ((__BIT(MAX((__m), (__n)) + 1) - 1) ^ (__BIT(MIN((__m), (__n))) - 1))

/* Last byte of physical address space. */
#define	ADM5120_TOP			0x1fffffff
#define	ADM5120_BOTTOM			0x0

/* Flash addresses */
#define	ADM5120_BASE_SRAM0		0x1fc00000

/* UARTs */
#define ADM5120_BASE_UART1		0x12800000
#define ADM5120_BASE_UART0		0x12600000

/* ICU */
#define	ADM5120_BASE_ICU		0x12200000
#define		ICU_STATUS_REG		0x00
#define		ICU_RAW_STATUS_REG	0x04
#define		ICU_ENABLE_REG		0x08
#define		ICU_DISABLE_REG		0x0c
#define		ICU_SOFT_REG		0x10
#define		ICU_MODE_REG		0x14
#define		ICU_FIQ_STATUS_REG	0x18
#define		ICU_TESTSRC_REG		0x1c
#define		ICU_SRCSEL_REG		0x20
#define		ICU_LEVEL_REG		0x24
#define		ICU_INT_MASK		0x3ff

/* Switch */
#define	ADM5120_BASE_SWITCH		0x12000000
#define		SW_CODE_REG		0x00
#define			CLKS_MASK		0x00300000
#define			CLKS_175MHZ		0x00000000
#define			CLKS_200MHZ		0x00100000
#define		SW_SFTRES_REG		0x04
#define		SW_MEMCONT_REG		0x1c
#define			SDRAM_SIZE_4MBYTES	0x0001
#define			SDRAM_SIZE_8MBYTES	0x0002
#define			SDRAM_SIZE_16MBYTES	0x0003
#define			SDRAM_SIZE_64MBYTES	0x0004
#define			SDRAM_SIZE_128MBYTES	0x0005
#define			SDRAM_SIZE_MASK		0x0007
#define			SRAM0_SIZE_SHIFT	8
#define			SRAM1_SIZE_SHIFT	16
#define			SRAM_MASK		0x0007
#define			SRAM_SSIZE		0x40000

#define	ADM5120_BASE_PCI_CONFDATA	0x115ffff8
#define	ADM5120_BASE_PCI_CONFADDR	0x115ffff0
#define	ADM5120_BASE_PCI_IO		0x11500000
#define	ADM5120_BASE_PCI_MEM		0x11400000
#define	ADM5120_BASE_USB		0x11200000
#define	ADM5120_BASE_MPMC		0x11000000
#define	ADM5120_BASE_EXTIO1		0x10e00000
#define	ADM5120_BASE_EXTIO0		0x10c00000
#define	ADM5120_BASE_RSVD0		0x10800000
#define	ADM5120_BASE_SRAM1		0x10000000

#define	_REG_READ(b, o)	*((volatile uint32_t *)MIPS_PHYS_TO_KSEG1((b) + (o)))
#define	SW_READ(o)	_REG_READ(ADM5120_BASE_SWITCH, o)

#define	_REG_WRITE(b, o, v)	(_REG_READ(b, o)) = (v)
#define	SW_WRITE(o, v)	_REG_WRITE(ADM5120_BASE_SWITCH,o, v)

/* USB */

/* Watchdog Timers: base address is switch controller */

#define	ADM5120_WDOG0			0x00c0
#define	ADM5120_WDOG1			0x00c4

#define	ADM5120_WDOG0_WTTR	__BIT(31)	/* 0: do not reset,
						 * 1: reset on wdog expiration
						 */
#define	ADM5120_WDOG1_WDE	__BIT(31)	/* 0: deactivate,
						 * 1: drop all CPU-bound
						 * packets, disable flow
						 * control on all ports.
						 */
#define	ADM5120_WDOG_WTS_MASK	__BITS(30, 16)	/* Watchdog Timer Set:
						 * timer expires when it
						 * reaches WTS.  Units of
						 * 10ms.
						 */
#define	ADM5120_WDOG_RSVD	__BIT(15)
#define	ADM5120_WDOG_WT_MASK	__BITS(14, 0)	/* Watchdog Timer:
						 * counts up, write to clear.
						 */

/* GPIO: base address is switch controller */
#define	ADM5120_GPIO0			0x00b8

#define	ADM5120_GPIO0_OV	__BITS(31, 24)	/* rw: output value */
#define	ADM5120_GPIO0_OE	__BITS(23, 16)	/* rw: output enable,
						 * bit[n] = 0 -> input
						 * bit[n] = 1 -> output
						 */
#define	ADM5120_GPIO0_IV	__BITS(15, 8)	/* ro: input value */
#define	ADM5120_GPIO0_RSVD	__BITS(7, 0)	/* rw: reserved */

#define	ADM5120_GPIO2			0x00bc
#define	ADM5120_GPIO2_EW	__BIT(6)	/* 1: enable wait state pin,
						 * pin GPIO[0], for GPIO[1]
						 * or GPIO[3] Chip Select:
						 * memory controller waits for
						 * WAIT# inactive (high).
						 */
#define	ADM5120_GPIO2_CSX1	__BIT(5)	/* 1: GPIO[3:4] act as
						 * Chip Select for
						 * External I/O 1 (CSX1)
						 * and External Interrupt 1
						 * (INTX1), respectively.
						 * 0: CSX1/INTX1 disabled
						 */
#define	ADM5120_GPIO2_CSX0	__BIT(4)	/* 1: GPIO[1:2] act as
						 * Chip Select for
						 * External I/O 0 (CSX0)
						 * and External Interrupt 0
						 * (INTX0), respectively.
						 * 0: CSX0/INTX0 disabled
						 */

/* MultiPort Memory Controller (MPMC) */

#define	ADM5120_MPMC_CONTROL	0x000
#define	ADM5120_MPMC_CONTROL_DWB	__BIT(3)	/* write 1 to
							 * drain write
							 * buffers.  write 0
							 * for normal buffer
							 * operation.
							 */
#define	ADM5120_MPMC_CONTROL_LPM	__BIT(2)	/* 1: activate low-power
							 * mode.  SDRAM is
							 * still refreshed.
							 */
#define	ADM5120_MPMC_CONTROL_AM		__BIT(1)	/* 1: address mirror:
							 * static memory
							 * chip select 0
							 * is mapped to chip
							 * select 1.
							 */
#define	ADM5120_MPMC_CONTROL_ME		__BIT(0)	/* 0: disable MPMC.
							 * DRAM is not
							 * refreshed.
							 * 1: enable MPMC.
							 */

#define	ADM5120_MPMC_STATUS	0x004
#define	ADM5120_MPMC_STATUS_SRA		__BIT(2)	/* read-only
							 * MPMC operating mode
							 * indication,
							 * 1: self-refresh
							 * acknowledge
							 * 0: normal mode
							 */
#define	ADM5120_MPMC_STATUS_WBS		__BIT(1)	/* read-only
							 * write-buffer status,
							 * 0: buffers empty
							 * 1: contain data
							 */
#define	ADM5120_MPMC_STATUS_BU		__BIT(0)	/* read-only MPMC
							 * "busy" indication,
							 * 0: MPMC idle
							 * 1: MPMC is performing
							 * memory transactions
							 */

#define	ADM5120_MPMC_SEW	0x080
#define	ADM5120_MPMC_SEW_RSVD	__BITS(31, 10)
#define	ADM5120_MPMC_SEW_EWTO	__BITS(9, 0)	/* timeout access after
						 * 16 * (n + 1) clock cycles
						 * (XXX which clock?)
						 */

#define	ADM5120_MPMC_SC(__i)	(0x200 + 0x020 * (__i))
#define	ADM5120_MPMC_SC_RSVD0	__BITS(31, 21)
#define	ADM5120_MPMC_SC_WP	__BIT(20)	/* 1: write protect */
#define	ADM5120_MPMC_SC_BE	__BIT(20)	/* 1: enable write buffer */
#define	ADM5120_MPMC_SC_RSVD1	__BITS(18, 9)
#define	ADM5120_MPMC_SC_EW	__BIT(8)	/* 1: enable extended wait;
						 */
#define	ADM5120_MPMC_SC_BLS	__BIT(7)	/* 0: byte line state pins
						 * are active high on read,
						 * active low on write.
						 *
						 * 1: byte line state pins
						 * are active low on read and
						 * on write.
						 */
#define	ADM5120_MPMC_SC_CCP	__BIT(6)	/* 0: chip select is active low,
						 * 1: active high
						 */
#define	ADM5120_MPMC_SC_RSVD2	__BITS(5, 4)
#define	ADM5120_MPMC_SC_PM	__BIT(3)	/* 0: page mode disabled,
						 * 1: enable asynchronous
						 * page mode four
						 */
#define	ADM5120_MPMC_SC_RSVD3	__BIT(2)
#define	ADM5120_MPMC_SC_MW_MASK	__BITS(1, 0)	/* memory width, bits */
#define	ADM5120_MPMC_SC_MW_8B	__SHIFTIN(0, ADM5120_MPMC_SC_MW_MASK)
#define	ADM5120_MPMC_SC_MW_16B	__SHIFTIN(1, ADM5120_MPMC_SC_MW_MASK)
#define	ADM5120_MPMC_SC_MW_32B	__SHIFTIN(2, ADM5120_MPMC_SC_MW_MASK)
#define	ADM5120_MPMC_SC_MW_RSVD	__SHIFTIN(3, ADM5120_MPMC_SC_MW_MASK)

#define	ADM5120_MPMC_SWW(__i)	(0x204 + 0x020 * (__i))
#define	ADM5120_MPMC_SWW_RSVD	__BITS(31, 4)
#define	ADM5120_MPMC_SWW_WWE	__BITS(3, 0)	/* delay (n + 1) * HCLK cycles
						 * after asserting chip select
						 * (CS) before asserting write
						 * enable (WE)
						 */

#define	ADM5120_MPMC_SWO(__i)	(0x208 + 0x020 * (__i))
#define	ADM5120_MPMC_SWO_RSVD	__BITS(31, 4)
#define	ADM5120_MPMC_SWO_WOE	__BITS(3, 0)	/* delay n * HCLK cycles
						 * after asserting chip select
						 * before asserting output
						 * enable (OE)
						 */

#define	ADM5120_MPMC_SWR(__i)	(0x20c + 0x020 * (__i))
#define	ADM5120_MPMC_SWR_RSVD	__BITS(31, 5)
#define	ADM5120_MPMC_SWR_NMRW	__BITS(4, 0)	/* read wait states for
						 * either first page-mode
						 * access or for non-page mode
						 * read, (n + 1) * HCLK cycles
						 */

#define	ADM5120_MPMC_SWP(__i)	(0x210 + 0x020 * (__i))
#define	ADM5120_MPMC_SWP_RSVD	__BITS(31, 5)
#define	ADM5120_MPMC_SWP_WPS	__BITS(4, 0)	/* read wait states for
						 * second and subsequent
						 * page-mode read,
						 * (n + 1) * HCLK cycles
						 */

#define	ADM5120_MPMC_SWWR(__i)	(0x214 + 0x020 * (__i))
#define	ADM5120_MPMC_SWWR_RSVD	__BITS(31, 5)
#define	ADM5120_MPMC_SWWR_WWS	__BITS(4, 0)	/* write wait states after
						 * the first read (??),
						 * (n + 2) * HCLK cycles
						 */

#define	ADM5120_MPMC_SWT(__i)	(0x218 + 0x020 * (__i))
#define	ADM5120_MPMC_SWT_RSVD		__BITS(31, 4)
#define	ADM5120_MPMC_SWT_WAITTURN	__BITS(3, 0)	/* bus turnaround time,
							 * (n + 1) * HCLK cycles
							 */

#endif /* _ADM5120REG_H_ */
