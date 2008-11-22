/*-
 * Copyright (c) 1999, 2000 Matthew R. Green
 * Copyright (c) 2001 - 2003 by Thomas Moestl <tmm@FreeBSD.org>
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: NetBSD: psycho.c,v 1.35 2001/09/10 16:17:06 eeh Exp
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_ofw_pci.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/bus.h>

#include <dev/ofw/openfirm.h>

#include <machine/bus.h>

#include <sparc64/pci/ofw_pci.h>

static uint8_t pci_bus_cnt;
static phandle_t *pci_bus_map;
static int pci_bus_map_sz;

#define	PCI_BUS_MAP_INC	10

uint8_t
ofw_pci_alloc_busno(phandle_t node)
{
	phandle_t *om;
	int osz;
	uint8_t n;

	n = pci_bus_cnt++;
	/* Establish a mapping between bus numbers and device nodes. */
	if (n >= pci_bus_map_sz) {
		osz = pci_bus_map_sz;
		om = pci_bus_map;
		pci_bus_map_sz = n + PCI_BUS_MAP_INC;
		pci_bus_map = malloc(sizeof(*pci_bus_map) * pci_bus_map_sz,
		    M_DEVBUF, M_WAITOK | M_ZERO);
		if (om != NULL) {
			bcopy(om, pci_bus_map, sizeof(*om) * osz);
			free(om, M_DEVBUF);
		}
	}
	pci_bus_map[n] = node;
	return (n);
}
