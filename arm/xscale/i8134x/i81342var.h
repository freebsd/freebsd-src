/*-
 * Copyright (c) 2006 Olivier Houchard
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/* $FreeBSD$ */
#ifndef I81342VAR_H_
#define I81342VAR_H_

#include <sys/rman.h>

struct i81342_softc {
	device_t dev;
	bus_space_tag_t 	sc_st;
	bus_space_handle_t 	sc_sh;
	bus_space_handle_t	sc_atux_sh;
	bus_space_handle_t	sc_atue_sh;
	bus_space_tag_t		sc_pciio;
	bus_space_tag_t		sc_pcimem;
	struct rman 		sc_irq_rman;
};

struct i81342_pci_map {
	vm_offset_t vaddr;
	vm_paddr_t paddr;
	vm_size_t size;
	struct i81342_pci_map *next;
};

struct i81342_pci_softc {
	device_t		sc_dev;
	bus_space_tag_t		sc_st;
	bus_space_handle_t	sc_atu_sh;
	struct bus_space	sc_pciio;
	struct bus_space	sc_pcimem;
	struct rman		sc_mem_rman;
	struct rman		sc_io_rman;
	struct rman		sc_irq_rman;
	char			sc_is_atux;
	int			sc_busno;
	struct i81342_pci_map	*sc_pci_mappings;
};

void i81342_bs_init(bus_space_tag_t, void *);
void i81342_io_bs_init(bus_space_tag_t, void *);
void i81342_mem_bs_init(bus_space_tag_t, void *);
void i81342_sdram_bounds(bus_space_tag_t, bus_space_handle_t, vm_paddr_t *,
    vm_size_t *);
#endif /*I81342VAR_H_ */
