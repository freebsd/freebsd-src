/*	$NetBSD: i80321_mainbus.c,v 1.13 2003/12/17 22:03:24 abs Exp $	*/

/*-
 * Copyright (c) 2001, 2002 Wasabi Systems, Inc.
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
 */

/*
 * IQ80321 front-end for the i80321 I/O Processor.  We take care
 * of setting up the i80321 memory map, PCI interrupt routing, etc.,
 * which are all specific to the board the i80321 is wired up to.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#define _ARM32_BUS_DMA_PRIVATE
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/malloc.h>
#include <sys/rman.h>
#include <machine/bus.h>
#include <machine/intr.h>

#include <arm/xscale/i80321/i80321reg.h>
#include <arm/xscale/i80321/i80321var.h>
#include <arm/xscale/i80321/iq80321reg.h>
#include <arm/xscale/i80321/iq80321var.h>
#include <arm/xscale/i80321/i80321_intr.h>

#include <dev/pci/pcireg.h>


int	iq80321_probe(device_t);
void	iq80321_identify(driver_t *, device_t);
int	iq80321_attach(device_t);

int
iq80321_probe(device_t dev)
{
	device_set_desc(dev, "Intel 80321");
	return (0);
}

void
iq80321_identify(driver_t *driver, device_t parent)
{
	
	BUS_ADD_CHILD(parent, 0, "iq", 0);
}

static struct arm32_dma_range i80321_dr;
static int dma_range_init = 0;

struct arm32_dma_range *
bus_dma_get_range(void)
{
	if (dma_range_init == 0)
		return (NULL);
	return (&i80321_dr);
}

int
bus_dma_get_range_nb(void)
{
	if (dma_range_init == 0)
		return (0);
	return (1);
}

#define PCI_MAPREG_MEM_PREFETCHABLE_MASK	0x00000008
#define PCI_MAPREG_MEM_TYPE_64BIT		0x00000004

int
iq80321_attach(device_t dev)
{
	struct i80321_softc *sc = device_get_softc(dev);
	int b0u, b0l, b1u, b1l;
	vm_paddr_t memstart = 0;
	vm_size_t memsize = 0;
	int busno;

	/*
	 * Fill in the space tag for the i80321's own devices,
	 * and hand-craft the space handle for it (the device
	 * was mapped during early bootstrap).
	 */
	i80321_bs_init(&i80321_bs_tag, sc);
	sc->sc_st = &i80321_bs_tag;
	sc->sc_sh = IQ80321_80321_VBASE;
	sc->dev = dev;
	sc->sc_is_host = 1;

	/*
	 * Slice off a subregion for the Memory Controller -- we need it
	 * here in order read the memory size.
	 */
	if (bus_space_subregion(sc->sc_st, sc->sc_sh, VERDE_MCU_BASE,
	    VERDE_MCU_SIZE, &sc->sc_mcu_sh))
		panic("%s: unable to subregion MCU registers",
		    device_get_name(dev));

	if (bus_space_subregion(sc->sc_st, sc->sc_sh, VERDE_ATU_BASE,
	    VERDE_ATU_SIZE, &sc->sc_atu_sh))
		panic("%s: unable to subregion ATU registers",
		    device_get_name(dev));

	/*
	 * We have mapped the PCI I/O windows in the early
	 * bootstrap phase.
	 */
	sc->sc_iow_vaddr = IQ80321_IOW_VBASE;

	/*
	 * Check the configuration of the ATU to see if another BIOS
	 * has configured us.  If a PC BIOS didn't configure us, then:
	 * 	IQ80321: BAR0 00000000.0000000c BAR1 is 00000000.8000000c.
	 * 	IQ31244: BAR0 00000000.00000004 BAR1 is 00000000.0000000c.
	 * If a BIOS has configured us, at least one of those should be
	 * different.  This is pretty fragile, but it's not clear what
	 * would work better.
	 */
	b0l = bus_space_read_4(sc->sc_st, sc->sc_atu_sh, PCIR_BARS+0x0);
	b0u = bus_space_read_4(sc->sc_st, sc->sc_atu_sh, PCIR_BARS+0x4);
	b1l = bus_space_read_4(sc->sc_st, sc->sc_atu_sh, PCIR_BARS+0x8);
	b1u = bus_space_read_4(sc->sc_st, sc->sc_atu_sh, PCIR_BARS+0xc);

#ifdef VERBOSE_INIT_ARM	
	printf("i80321: BAR0 = %08x.%08x BAR1 = %08x.%08x\n",
		   b0l,b0u, b1l, b1u );
#endif

#define PCI_MAPREG_MEM_ADDR_MASK	0xfffffff0
	b0l &= PCI_MAPREG_MEM_ADDR_MASK;
	b0u &= PCI_MAPREG_MEM_ADDR_MASK;
	b1l &= PCI_MAPREG_MEM_ADDR_MASK;
	b1u &= PCI_MAPREG_MEM_ADDR_MASK;

#ifdef VERBOSE_INIT_ARM	
	printf("i80219: BAR0 = %08x.%08x BAR1 = %08x.%08x\n",
		   b0l,b0u, b1l, b1u );
#endif

	if ((b0u != b1u) || (b0l != 0) || ((b1l & ~0x80000000U) != 0))
		sc->sc_is_host = 0;
	else
		sc->sc_is_host = 1;

	/* FIXME: i force it's */	

#ifdef CPU_XSCALE_80219
	sc->sc_is_host = 1;
#endif
	
	i80321_sdram_bounds(sc->sc_st, sc->sc_mcu_sh, &memstart, &memsize);
	/*
	 * We set up the Inbound Windows as follows:
	 *
	 *	0	Access to i80321 PMMRs
	 *
	 *	1	Reserve space for private devices
	 *
	 *	2	RAM access
	 *
	 *	3	Unused.
	 *
	 * This chunk needs to be customized for each IOP321 application.
	 */
#if 0
	sc->sc_iwin[0].iwin_base_lo = VERDE_PMMR_BASE;
	sc->sc_iwin[0].iwin_base_hi = 0;
	sc->sc_iwin[0].iwin_xlate = VERDE_PMMR_BASE;
	sc->sc_iwin[0].iwin_size = VERDE_PMMR_SIZE;
#endif
	if (sc->sc_is_host) {
		
		/* Map PCI:Local 1:1. */
		sc->sc_iwin[1].iwin_base_lo = VERDE_OUT_XLATE_MEM_WIN0_BASE |
		    PCI_MAPREG_MEM_PREFETCHABLE_MASK |
		    PCI_MAPREG_MEM_TYPE_64BIT;
		sc->sc_iwin[1].iwin_base_hi = 0;
	} else {
		
		sc->sc_iwin[1].iwin_base_lo = 0;
		sc->sc_iwin[1].iwin_base_hi = 0;
	}
	sc->sc_iwin[1].iwin_xlate = VERDE_OUT_XLATE_MEM_WIN0_BASE;
	sc->sc_iwin[1].iwin_size = VERDE_OUT_XLATE_MEM_WIN_SIZE;
	
	if (sc->sc_is_host) {
		sc->sc_iwin[2].iwin_base_lo = memstart |
		    PCI_MAPREG_MEM_PREFETCHABLE_MASK |
		    PCI_MAPREG_MEM_TYPE_64BIT;
		sc->sc_iwin[2].iwin_base_hi = 0;
	} else {
		sc->sc_iwin[2].iwin_base_lo = 0;
		sc->sc_iwin[2].iwin_base_hi = 0;
	}
	sc->sc_iwin[2].iwin_xlate = memstart;
	sc->sc_iwin[2].iwin_size = memsize;

	if (sc->sc_is_host) {
		sc->sc_iwin[3].iwin_base_lo = 0 |
		    PCI_MAPREG_MEM_PREFETCHABLE_MASK |
		    PCI_MAPREG_MEM_TYPE_64BIT;
	} else {
		sc->sc_iwin[3].iwin_base_lo = 0;
	}
	sc->sc_iwin[3].iwin_base_hi = 0;
	sc->sc_iwin[3].iwin_xlate = 0;
	sc->sc_iwin[3].iwin_size = 0;
	
#ifdef 	VERBOSE_INIT_ARM
	printf("i80321: Reserve space for private devices (Inbound Window 1) \n hi:0x%08x lo:0x%08x xlate:0x%08x size:0x%08x\n",
		   sc->sc_iwin[1].iwin_base_hi,
		   sc->sc_iwin[1].iwin_base_lo,
		   sc->sc_iwin[1].iwin_xlate,
		   sc->sc_iwin[1].iwin_size
		);
	printf("i80321: RAM access (Inbound Window 2) \n hi:0x%08x lo:0x%08x xlate:0x%08x size:0x%08x\n",
		   sc->sc_iwin[2].iwin_base_hi,
		   sc->sc_iwin[2].iwin_base_lo,
		   sc->sc_iwin[2].iwin_xlate,
		   sc->sc_iwin[2].iwin_size
		);
#endif

	/*
	 * We set up the Outbound Windows as follows:
	 *
	 *	0	Access to private PCI space.
	 *
	 *	1	Unused.
	 */
#define PCI_MAPREG_MEM_ADDR(x) ((x) & 0xfffffff0)
	sc->sc_owin[0].owin_xlate_lo =
    	    PCI_MAPREG_MEM_ADDR(sc->sc_iwin[1].iwin_base_lo);
	sc->sc_owin[0].owin_xlate_hi = sc->sc_iwin[1].iwin_base_hi;
	/*
	 * Set the Secondary Outbound I/O window to map
	 * to PCI address 0 for all 64K of the I/O space.
	 */
	sc->sc_ioout_xlate = 0;
	i80321_attach(sc);
	i80321_dr.dr_sysbase = sc->sc_iwin[2].iwin_xlate;
	i80321_dr.dr_busbase = PCI_MAPREG_MEM_ADDR(sc->sc_iwin[2].iwin_base_lo);
	i80321_dr.dr_len = sc->sc_iwin[2].iwin_size;
	dma_range_init = 1;
	busno = bus_space_read_4(sc->sc_st, sc->sc_atu_sh, ATU_PCIXSR);
	busno = PCIXSR_BUSNO(busno);
	if (busno == 0xff)
		busno = 0;
	sc->sc_irq_rman.rm_type = RMAN_ARRAY;
	sc->sc_irq_rman.rm_descr = "i80321 IRQs";
	if (rman_init(&sc->sc_irq_rman) != 0 ||
	    rman_manage_region(&sc->sc_irq_rman, 0, 25) != 0)
		panic("i80321_attach: failed to set up IRQ rman");

	device_add_child(dev, "obio", 0);
	device_add_child(dev, "itimer", 0);
	device_add_child(dev, "iopwdog", 0);
#ifndef 	CPU_XSCALE_80219
	device_add_child(dev, "iqseg", 0);
#endif	
	device_add_child(dev, "pcib", busno);
	device_add_child(dev, "i80321_dma", 0);
	device_add_child(dev, "i80321_dma", 1);
#ifndef CPU_XSCALE_80219	
	device_add_child(dev, "i80321_aau", 0);
#endif
	bus_generic_probe(dev);
	bus_generic_attach(dev);

	return (0);
}

void
arm_mask_irq(uintptr_t nb)
{
	intr_enabled &= ~(1 << nb);
	i80321_set_intrmask();
}

void
arm_unmask_irq(uintptr_t nb)
{
	intr_enabled |= (1 << nb);
	i80321_set_intrmask();
}


void
cpu_reset()
{
	(void) disable_interrupts(I32_bit|F32_bit);
	*(__volatile uint32_t *)(IQ80321_80321_VBASE + VERDE_ATU_BASE +
	    ATU_PCSR) = PCSR_RIB | PCSR_RPB;
	printf("Reset failed!\n");
	for(;;);
}

static struct resource *
iq80321_alloc_resource(device_t dev, device_t child, int type, int *rid,
    u_long start, u_long end, u_long count, u_int flags)
{
	struct i80321_softc *sc = device_get_softc(dev);
	struct resource *rv;

	if (type == SYS_RES_IRQ) {
		rv = rman_reserve_resource(&sc->sc_irq_rman,
		    start, end, count, flags, child);
		if (rv != NULL)
			rman_set_rid(rv, *rid);
		return (rv);
	}
	return (NULL);
}

static int
iq80321_setup_intr(device_t dev, device_t child,
    struct resource *ires, int flags, driver_filter_t *filt,
    driver_intr_t *intr, void *arg, void **cookiep)
{
	int error;

	error = BUS_SETUP_INTR(device_get_parent(dev), child, ires, flags,
	    filt, intr, arg, cookiep);
	if (error)
		return (error);
	intr_enabled |= 1 << rman_get_start(ires);
	i80321_set_intrmask();
	
	return (0);
}

static int
iq80321_teardown_intr(device_t dev, device_t child, struct resource *res,
    void *cookie)
{
	return (BUS_TEARDOWN_INTR(device_get_parent(dev), child, res, cookie));
}

static device_method_t iq80321_methods[] = {
	DEVMETHOD(device_probe, iq80321_probe),
	DEVMETHOD(device_attach, iq80321_attach),
	DEVMETHOD(device_identify, iq80321_identify),
	DEVMETHOD(bus_alloc_resource, iq80321_alloc_resource),
	DEVMETHOD(bus_setup_intr, iq80321_setup_intr),
	DEVMETHOD(bus_teardown_intr, iq80321_teardown_intr),
	{0, 0},
};

static driver_t iq80321_driver = {
	"iq",
	iq80321_methods,
	sizeof(struct i80321_softc),
};
static devclass_t iq80321_devclass;

DRIVER_MODULE(iq, nexus, iq80321_driver, iq80321_devclass, 0, 0);
