/*	$NetBSD: i80321var.h,v 1.8 2003/10/06 16:06:06 thorpej Exp $	*/

/*-
 * Copyright (c) 2002, 2003 Wasabi Systems, Inc.
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
 * $FreeBSD: src/sys/arm/xscale/i80321/i80321var.h,v 1.5.6.1 2008/11/25 02:59:29 kensmith Exp $
 *
 */

#ifndef _ARM_XSCALE_I80321VAR_H_
#define	_ARM_XSCALE_I80321VAR_H_

#include <sys/queue.h>
#include <dev/pci/pcivar.h>
#include <sys/rman.h>

extern struct bus_space i80321_bs_tag;

struct i80321_softc {
	device_t 		dev;
	bus_space_tag_t 	sc_st;
	bus_space_handle_t	sc_sh;
	/* Handles for the various subregions. */
	bus_space_handle_t 	sc_atu_sh;
	bus_space_handle_t 	sc_mcu_sh;
	int			sc_is_host;

	/*
	 * We expect the board-specific front-end to have already mapped
	 * the PCI I/O space .. it is only 64K, and I/O mappings tend to
	 * be smaller than a page size, so it's generally more efficient
	 * to map them all into virtual space in one fell swoop.
	 */
	vm_offset_t		sc_iow_vaddr;		/* I/O window vaddr */

	/*
	 * Variables that define the Inbound windows.  The base address of
	 * 0-2 are configured by a host via BARs.  The xlate variable
	 * defines the start of the local address space that it maps to.
	 * The size variable defines the byte size.
	 *
	 * The first 3 windows are for incoming PCI memory read/write
	 * cycles from a host.  The 4th window, not configured by the
	 * host (as it outside the normal BAR range) is the inbound
	 * window for PCI devices controlled by the i80321.
	 */
	struct {
		uint32_t iwin_base_hi;
		uint32_t iwin_base_lo;
		uint32_t iwin_xlate;
		uint32_t iwin_size;
	} sc_iwin[4];

	/*
	 * Variables that define the Outbound windows.
	 */
	struct {
		uint32_t owin_xlate_lo;
		uint32_t owin_xlate_hi;
	} sc_owin[2];

	/*
	 * This is the PCI address that the Outbound I/O
	 * window maps to.
	 */
	uint32_t		sc_ioout_xlate;

	/* Bus space, DMA, and PCI tags for the PCI bus (private devices). */
	struct bus_space 	sc_pci_iot;
	struct bus_space 	sc_pci_memt;

	/* GPIO state */
	uint8_t sc_gpio_dir;    /* GPIO pin direction (1 == output) */
	uint8_t sc_gpio_val;    /* GPIO output pin value */
	struct rman sc_irq_rman;
			
};


struct i80321_pci_softc {
	device_t 		sc_dev;
	bus_space_tag_t 	sc_st;
	bus_space_handle_t 	sc_atu_sh;
	bus_space_tag_t		sc_pciio;
	bus_space_tag_t		sc_pcimem;
	int			sc_busno;
	struct rman		sc_mem_rman;
	struct rman		sc_io_rman;
	struct rman		sc_irq_rman;
	uint32_t		sc_mem;
	uint32_t		sc_io;
};

void	i80321_sdram_bounds(bus_space_tag_t, bus_space_handle_t,
	    vm_paddr_t *, vm_size_t *);

void	i80321_attach(struct i80321_softc *);
void	i80321_calibrate_delay(void);

void	i80321_bs_init(bus_space_tag_t, void *);
void	i80321_io_bs_init(bus_space_tag_t, void *);
void	i80321_mem_bs_init(bus_space_tag_t, void *);
extern int machdep_pci_route_interrupt(device_t pcib, device_t dev, int pin);


#endif /* _ARM_XSCALE_I80321VAR_H_ */
