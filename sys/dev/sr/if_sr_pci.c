/*-
 * Copyright (c) 1996 - 2001 John Hay.
 * Copyright (c) 1996 SDL Communications, Inc.
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
 * 3. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <machine/bus_pio.h>
#include <machine/bus_memio.h>
#include <sys/rman.h>

#include <dev/pci/pcivar.h>
#include <machine/md_var.h>

#include <dev/ic/hd64570.h>
#include <dev/sr/if_srregs.h>

#ifndef BUGGY
#define BUGGY		0
#endif

static int	sr_pci_probe(device_t);
static int	sr_pci_attach(device_t);

static device_method_t sr_pci_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		sr_pci_probe),
	DEVMETHOD(device_attach,	sr_pci_attach),
	DEVMETHOD(device_detach,	sr_detach),
	{ 0, 0 }
};

static driver_t sr_pci_driver = {
	"sr",
	sr_pci_methods,
	sizeof(struct sr_hardc),
};

DRIVER_MODULE(sr, pci, sr_pci_driver, sr_devclass, 0, 0);
MODULE_DEPEND(sr, pci, 1, 1, 1);

static u_int	src_get8_mem(struct sr_hardc *hc, u_int off);
static u_int	src_get16_mem(struct sr_hardc *hc, u_int off);
static void	src_put8_mem(struct sr_hardc *hc, u_int off, u_int val);
static void	src_put16_mem(struct sr_hardc *hc, u_int off, u_int val);

static int
sr_pci_probe(device_t device)
{
	u_int32_t	type = pci_get_devid(device);

	switch(type) {
	case 0x556812aa:
		device_set_desc(device, "RISCom/N2pci");
		return (0);
		break;
	case 0x55684778:
	case 0x55684877:
		/*
		 * XXX This can probably be removed sometime.
		 */
		device_set_desc(device, "RISCom/N2pci (old id)");
		return (0);
		break;
	default:
		break;
	}
	return (ENXIO);
}

static int
sr_pci_attach(device_t device)
{
	int numports;
	u_int fecr;
	struct sr_hardc *hc;
	bus_space_tag_t bt_plx;
	bus_space_handle_t bh_plx;

	hc = (struct sr_hardc *)device_get_softc(device);
	bzero(hc, sizeof(struct sr_hardc));

	if (sr_allocate_plx_memory(device, 0x10, 1))
		goto errexit;
	bt_plx = rman_get_bustag(hc->res_plx_memory);
	bh_plx = rman_get_bushandle(hc->res_plx_memory);

	if (sr_allocate_memory(device, 0x18, 1))
		goto errexit;

	if (sr_allocate_irq(device, 0, 1))
		goto errexit;

	hc->cunit = device_get_unit(device);

	/*
	 * Configure the PLX. This is magic. I'm doing it just like I'm told
	 * to. :-)
	 * 
	 * offset
	 *  0x00 - Map Range    - Mem-mapped to locate anywhere
	 *  0x04 - Re-Map       - PCI address decode enable
	 *  0x18 - Bus Region   - 32-bit bus, ready enable
	 *  0x1c - Master Range - include all 16 MB
	 *  0x20 - Master RAM   - Map SCA Base at 0
	 *  0x28 - Master Remap - direct master memory enable
	 *  0x68 - Interrupt    - Enable interrupt (0 to disable)
	 * 
	 * Note: This is "cargo cult" stuff.  - jrc
	 */
	bus_space_write_4(bt_plx, bh_plx, 0x00, 0xfffff000);
	bus_space_write_4(bt_plx, bh_plx, 0x04, 0x00000001);
	bus_space_write_4(bt_plx, bh_plx, 0x18, 0x40030043);
	bus_space_write_4(bt_plx, bh_plx, 0x1c, 0xff000000);
	bus_space_write_4(bt_plx, bh_plx, 0x20, 0x00000000);
	bus_space_write_4(bt_plx, bh_plx, 0x28, 0x000000e9);
	bus_space_write_4(bt_plx, bh_plx, 0x68, 0x00010900);

	/*
	 * Get info from card.
	 *
	 * Only look for the second port if the first exists. Too many things
	 * will break if we have only a second port.
	 */
	fecr = sr_read_fecr(hc);
	numports = 0;

	if (((fecr & SR_FECR_ID0) >> SR_FE_ID0_SHFT) != SR_FE_ID_NONE) {
		numports++;
		if (((fecr & SR_FECR_ID1) >> SR_FE_ID1_SHFT) != SR_FE_ID_NONE)
			numports++;
	}
	if (numports == 0)
		goto errexit;

	hc->numports = numports;
	hc->cardtype = SR_CRD_N2PCI;

	hc->src_put8 = src_put8_mem;
	hc->src_put16 = src_put16_mem;
	hc->src_get8 = src_get8_mem;
	hc->src_get16 = src_get16_mem;

	/*
	 * Malloc area for tx and rx buffers. For now allocate SRC_WIN_SIZ
	 * (16k) for each buffer.
	 *
	 * Allocate the block below 16M because the N2pci card can only access
	 * 16M memory at a time.
	 *
	 * (We could actually allocate a contiguous block above the 16MB limit,
	 * but this would complicate card programming more than we want to
	 * right now -jrc)
	 */
	hc->memsize = 2 * hc->numports * SRC_WIN_SIZ;
	hc->mem_start = contigmalloc(hc->memsize,
				     M_DEVBUF,
				     M_NOWAIT,
				     0ul,
				     0xfffffful,
				     0x10000,
				     0x1000000);

	if (hc->mem_start == NULL) {
		printf("src%d: pci: failed to allocate buffer space.\n",
		    hc->cunit);
		goto errexit;
	}
	hc->winmsk = 0xffffffff;
	hc->mem_end = (caddr_t)((u_int)hc->mem_start + hc->memsize);
	hc->mem_pstart = kvtop(hc->mem_start);
	bzero(hc->mem_start, hc->memsize);

	sr_write_fecr(hc,
	    SR_FECR_DTR0 | SR_FECR_DTR1 | SR_FECR_TE0 | SR_FECR_TE1);

	if (sr_attach(device))
		goto errexit;

	return (0);
errexit:
	sr_deallocate_resources(device);
	return (ENXIO);
}

/*
 * I/O for PCI N2 card(s)
 */
#define SRC_PCI_SCA_REG(y)	((y & 2) ? ((y & 0xfd) + 0x100) : y)

static u_int
src_get8_mem(struct sr_hardc *hc, u_int off)
{
	return bus_space_read_1(hc->bt_memory, hc->bh_memory,
	    SRC_PCI_SCA_REG(off));
}

static u_int
src_get16_mem(struct sr_hardc *hc, u_int off)
{
	return bus_space_read_2(hc->bt_memory, hc->bh_memory,
	    SRC_PCI_SCA_REG(off));
}

static void
src_put8_mem(struct sr_hardc *hc, u_int off, u_int val)
{
	bus_space_write_1(hc->bt_memory, hc->bh_memory,
	    SRC_PCI_SCA_REG(off), val);
}

static void
src_put16_mem(struct sr_hardc *hc, u_int off, u_int val)
{
	bus_space_write_2(hc->bt_memory, hc->bh_memory,
	    SRC_PCI_SCA_REG(off), val);
}
