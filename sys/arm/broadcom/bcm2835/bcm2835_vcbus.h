/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2012 Oleksandr Tymoshenko <gonzo@freebsd.org>
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
 * $FreeBSD$
 */

/*
 * Defines for converting physical address to VideoCore bus address and back
 */

#ifndef _BCM2835_VCBUS_H_
#define _BCM2835_VCBUS_H_

#define	BCM2835_VCBUS_SDRAM_CACHED	0x40000000
#define	BCM2835_VCBUS_SDRAM_UNCACHED	0xC0000000

#define	BCM2835_ARM_IO_BASE		0x20000000
#define	BCM2835_VCBUS_IO_BASE		0x7E000000
#define	BCM2835_VCBUS_SDRAM_BASE	BCM2835_VCBUS_SDRAM_CACHED

#define	BCM2837_ARM_IO_BASE		0x3f000000
#define	BCM2837_VCBUS_IO_BASE		BCM2835_VCBUS_IO_BASE
#define	BCM2837_VCBUS_SDRAM_BASE	BCM2835_VCBUS_SDRAM_UNCACHED

#define	BCM2838_ARM_IO_BASE		0xfe000000
#define	BCM2838_VCBUS_IO_BASE		BCM2835_VCBUS_IO_BASE
#define	BCM2838_VCBUS_SDRAM_BASE	BCM2835_VCBUS_SDRAM_UNCACHED

/*
 * Max allowed SDRAM mapping for most peripherals.  The Raspberry Pi 4 has more
 * than 1 GB of SDRAM, but only the lowest 1 GB is mapped into the "Legacy
 * Master view" of the address space accessible by the DMA engine.  Technically,
 * we can slide this window around to whatever similarly sized range is
 * convenient, but this is the most useful window given how busdma(9) works and
 * that the window must be reconfigured for all channels in a given DMA engine.
 * The DMA lite engine's window can be configured separately from the 30-bit DMA
 * engine.
 */
#define	BCM2838_PERIPH_MAXADDR		0x3fffffff

#define	BCM28XX_ARM_IO_SIZE		0x01000000

vm_paddr_t bcm283x_armc_to_vcbus(vm_paddr_t pa);
vm_paddr_t bcm283x_vcbus_to_armc(vm_paddr_t vca);
bus_addr_t bcm283x_dmabus_peripheral_lowaddr(void);

#define	ARMC_TO_VCBUS(pa)	bcm283x_armc_to_vcbus(pa)
#define	VCBUS_TO_ARMC(vca)	bcm283x_vcbus_to_armc(vca)

/* Compatibility name for vchiq arm interface. */
#define	PHYS_TO_VCBUS		ARMC_TO_VCBUS

#endif /* _BCM2835_VCBUS_H_ */
