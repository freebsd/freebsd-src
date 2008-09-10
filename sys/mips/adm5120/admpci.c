/* $NetBSD: admpci.c,v 1.1 2007/03/20 08:52:02 dyoung Exp $ */

/*-
 * Copyright (c) 2007 David Young.  All rights reserved.
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
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 */
/*-
 * Copyright (c) 2006 Itronix Inc.
 * All rights reserved.
 *
 * Written by Garrett D'Amore for Itronix Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of Itronix Inc. may not be used to endorse
 *    or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY ITRONIX INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL ITRONIX INC. BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */ 

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>

#include <sys/bus.h>
#include <sys/interrupt.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/rman.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_extern.h>

#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/pmap.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>

#include <dev/pci/pcib_private.h>
#include "pcib_if.h"

#include <mips/adm5120/adm5120reg.h>

#ifdef ADMPCI_DEBUG
int admpci_debug = 1;
#define	ADMPCI_DPRINTF(__fmt, ...)		\
do {						\
	if (admpci_debug)			\
		printf((__fmt), __VA_ARGS__);	\
} while (/*CONSTCOND*/0)
#else /* !ADMPCI_DEBUG */
#define	ADMPCI_DPRINTF(__fmt, ...)	do { } while (/*CONSTCOND*/0)
#endif /* ADMPCI_DEBUG */

#define	ADMPCI_TAG_BUS_MASK		__BITS(23, 16)
/* Bit 11 is reserved.	It selects the AHB-PCI bridge.	Let device 0
 * be the bridge.  For all other device numbers, let bit[11] == 0.
 */
#define	ADMPCI_TAG_DEVICE_MASK		__BITS(15, 11)
#define	ADMPCI_TAG_DEVICE_SUBMASK	__BITS(15, 12)
#define	ADMPCI_TAG_DEVICE_BRIDGE	__BIT(11)
#define	ADMPCI_TAG_FUNCTION_MASK	__BITS(10, 8)
#define	ADMPCI_TAG_REGISTER_MASK	__BITS(7, 0)

#define	ADMPCI_MAX_DEVICE

struct admpci_softc {
	device_t		sc_dev;
	bus_space_tag_t		sc_st;

	/* Access to PCI config registers */
	bus_space_handle_t	sc_addrh;
	bus_space_handle_t	sc_datah;

	int			sc_busno;
	struct rman		sc_mem_rman;
	struct rman		sc_io_rman;
	struct rman		sc_irq_rman;
	uint32_t		sc_mem;
	uint32_t		sc_io;
};

static int
admpci_probe(device_t dev)
{

	return (0);
}

static int
admpci_attach(device_t dev)
{
	int busno = 0;
	struct admpci_softc *sc = device_get_softc(dev);

	sc->sc_dev = dev;
	sc->sc_busno = busno;

	/* Use KSEG1 to access IO ports for it is uncached */
	sc->sc_io = MIPS_PHYS_TO_KSEG1(ADM5120_BASE_PCI_IO);
	sc->sc_io_rman.rm_type = RMAN_ARRAY;
	sc->sc_io_rman.rm_descr = "ADMPCI I/O Ports";
	if (rman_init(&sc->sc_io_rman) != 0 ||
		rman_manage_region(&sc->sc_io_rman, 0, 0xffff) != 0) {
		panic("admpci_attach: failed to set up I/O rman");
	}

	/* Use KSEG1 to access PCI memory for it is uncached */
	sc->sc_mem = MIPS_PHYS_TO_KSEG1(ADM5120_BASE_PCI_MEM);
	sc->sc_mem_rman.rm_type = RMAN_ARRAY;
	sc->sc_mem_rman.rm_descr = "ADMPCI PCI Memory";
	if (rman_init(&sc->sc_mem_rman) != 0 ||
	    rman_manage_region(&sc->sc_mem_rman, 
	    sc->sc_mem, sc->sc_mem + 0x100000) != 0) {
		panic("admpci_attach: failed to set up memory rman");
	}

	sc->sc_irq_rman.rm_type = RMAN_ARRAY;
	sc->sc_irq_rman.rm_descr = "ADMPCI PCI IRQs";
	if (rman_init(&sc->sc_irq_rman) != 0 ||
	    rman_manage_region(&sc->sc_irq_rman, 1, 31) != 0)
		panic("admpci_attach: failed to set up IRQ rman");

	if (bus_space_map(sc->sc_st, ADM5120_BASE_PCI_CONFADDR, 4, 0, 
	    &sc->sc_addrh) != 0) {
		device_printf(sc->sc_dev, "unable to address space\n");
		panic("bus_space_map failed");
	}

	if (bus_space_map(sc->sc_st, ADM5120_BASE_PCI_CONFDATA, 4, 0, 
	    &sc->sc_datah) != 0) {
		device_printf(sc->sc_dev, "unable to address space\n");
		panic("bus_space_map failed");
	}

	device_add_child(dev, "pci", busno);
	return (bus_generic_attach(dev));
}

static int
admpci_maxslots(device_t dev)
{

	return (PCI_SLOTMAX);
}

static uint32_t
admpci_make_addr(int bus, int slot, int func, int reg)
{

	return (0x80000000 | (bus << 16) | (slot << 11) | (func << 8) | reg);
}

static uint32_t
admpci_read_config(device_t dev, int bus, int slot, int func, int reg,
    int bytes)
{
	struct admpci_softc *sc = device_get_softc(dev);
	uint32_t data;
	uint32_t shift, mask;
	bus_addr_t addr;

	ADMPCI_DPRINTF("%s: sc %p tag (%x, %x, %x) reg %d\n", __func__, 
			(void *)sc, bus, slot, func, reg);

	addr = admpci_make_addr(bus, slot, func, reg);

	ADMPCI_DPRINTF("%s: sc_addrh %p sc_datah %p addr %p\n", __func__,
	    (void *)sc->sc_addrh, (void *)sc->sc_datah, (void *)addr);

	bus_space_write_4(sc->sc_io, sc->sc_addrh, 0, addr);
	data = bus_space_read_4(sc->sc_io, sc->sc_datah, 0);

	switch (reg % 4) {
	case 3:
		shift = 24;
		break;
	case 2:
		shift = 16;
		break;
	case 1:
		shift = 8;
		break;
	default:
		shift = 0;
		break;
	}	

	switch (bytes) {
	case 1:
		mask = 0xff;
		data = (data >> shift) & mask;
		break;
	case 2:
		mask = 0xffff;
		if (reg % 4 == 0)
			data = data & mask;
		else
			data = (data >> 16) & mask;
		break;
	case 4:
		break;
	default:
		panic("%s: wrong bytes count", __func__);
		break;
	}

	ADMPCI_DPRINTF("%s: read 0x%x\n", __func__, data);
	return (data);
}

static void
admpci_write_config(device_t dev, int bus, int slot, int func, int reg,
    uint32_t data, int bytes)
{
	struct admpci_softc *sc = device_get_softc(dev);
	bus_addr_t addr;
	uint32_t reg_data;
	uint32_t shift, mask;

	ADMPCI_DPRINTF("%s: sc %p tag (%x, %x, %x) reg %d\n", __func__, 
			(void *)sc, bus, slot, func, reg);

	if (bytes != 4) {
		reg_data = admpci_read_config(dev, bus, slot, func, reg, 4);

		switch (reg % 4) {
		case 3:
			shift = 24;
			break;
		case 2:
			shift = 16;
			break;
		case 1:
			shift = 8;
			break;
		default:
			shift = 0;
			break;
		}	

		switch (bytes) {
		case 1:
			mask = 0xff;
			data = (reg_data & ~ (mask << shift)) | (data << shift);
			break;
		case 2:
			mask = 0xffff;
			if (reg % 4 == 0)
				data = (reg_data & ~mask) | data;
			else
				data = (reg_data & ~ (mask << shift)) | 
				    (data << shift);
			break;
		case 4:
			break;
		default:
			panic("%s: wrong bytes count", __func__);
			break;
		}
	}

	addr = admpci_make_addr(bus, slot, func, reg);

	ADMPCI_DPRINTF("%s: sc_addrh %p sc_datah %p addr %p\n", __func__,
	    (void *)sc->sc_addrh, (void *)sc->sc_datah, (void *)addr);

	bus_space_write_4(sc->sc_io, sc->sc_addrh, 0, addr);
	bus_space_write_4(sc->sc_io, sc->sc_datah, 0, data);
}

static int
admpci_route_interrupt(device_t pcib, device_t dev, int pin)
{
	/* TODO: implement */
	return (0);
}

static int
admpci_read_ivar(device_t dev, device_t child, int which, uintptr_t *result)
{
	struct admpci_softc *sc = device_get_softc(dev);

	switch (which) {
	case PCIB_IVAR_DOMAIN:
		*result = 0;
		return (0);
	case PCIB_IVAR_BUS:
		*result = sc->sc_busno;
		return (0);
	}

	return (ENOENT);
}

static int
admpci_write_ivar(device_t dev, device_t child, int which, uintptr_t result)
{
	struct admpci_softc * sc = device_get_softc(dev);

	switch (which) {
	case PCIB_IVAR_BUS:
		sc->sc_busno = result;
		return (0);
	}
	return (ENOENT);
}

static struct resource *
admpci_alloc_resource(device_t bus, device_t child, int type, int *rid,
    u_long start, u_long end, u_long count, u_int flags)
{

	return (NULL);
#if 0
	struct admpci_softc *sc = device_get_softc(bus);	
	struct resource *rv = NULL;
	struct rman *rm;
	bus_space_handle_t bh = 0;

	switch (type) {
	case SYS_RES_IRQ:
		rm = &sc->sc_irq_rman;
		break;
	case SYS_RES_MEMORY:
		rm = &sc->sc_mem_rman;
		bh = sc->sc_mem;
		break;
	case SYS_RES_IOPORT:
		rm = &sc->sc_io_rman;
		bh = sc->sc_io;
		break;
	default:
		return (NULL);
	}

	rv = rman_reserve_resource(rm, start, end, count, flags, child);
	if (rv == NULL)
		return (NULL);
	rman_set_rid(rv, *rid);
	if (type != SYS_RES_IRQ) {
		bh += (rman_get_start(rv));

		rman_set_bustag(rv, sc->sc_st);
		rman_set_bushandle(rv, bh);
		if (flags & RF_ACTIVE) {
			if (bus_activate_resource(child, type, *rid, rv)) {
				rman_release_resource(rv);
				return (NULL);
			}
		} 
	}
	return (rv);
#endif
}

static int
admpci_activate_resource(device_t bus, device_t child, int type, int rid,
    struct resource *r)
{
	bus_space_handle_t p;
	int error;
	
	if ((type == SYS_RES_MEMORY) || (type == SYS_RES_IOPORT)) {
		error = bus_space_map(rman_get_bustag(r),
		    rman_get_bushandle(r), rman_get_size(r), 0, &p);
		if (error) 
			return (error);
		rman_set_bushandle(r, p);
	}
	return (rman_activate_resource(r));
}

static int
admpci_setup_intr(device_t dev, device_t child, struct resource *ires, 
		int flags, driver_filter_t *filt, driver_intr_t *handler, 
		void *arg, void **cookiep)
{

#if 0
	struct admpci_softc *sc = device_get_softc(dev);
	struct intr_event *event;
	int irq, error;

	irq = rman_get_start(ires);
	if (irq >= ICU_LEN || irq == 2)
		panic("%s: bad irq or type", __func__);

	event = sc->sc_eventstab[irq];
	if (event == NULL) {
		error = intr_event_create(&event, (void *)irq, 0,
		    (void (*)(void *))NULL, "admpci intr%d:", irq);
		if (error)
			return 0;
		sc->sc_eventstab[irq] = event;
	}

	intr_event_add_handler(event, device_get_nameunit(child), filt, 
	    handler, arg, intr_priority(flags), flags, cookiep);

	/* Enable it, set trigger mode. */
	sc->sc_imask &= ~(1 << irq);
	sc->sc_elcr &= ~(1 << irq);

	admpci_set_icus(sc);
#endif

	return (0);
}

static int
admpci_teardown_intr(device_t dev, device_t child, struct resource *res,
    void *cookie)
{

	return (intr_event_remove_handler(cookie));
}

static device_method_t admpci_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		admpci_probe),
	DEVMETHOD(device_attach,	admpci_attach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	bus_generic_resume),

	/* Bus interface */
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_read_ivar,	admpci_read_ivar),
	DEVMETHOD(bus_write_ivar,	admpci_write_ivar),
	DEVMETHOD(bus_alloc_resource,	admpci_alloc_resource),
	DEVMETHOD(bus_release_resource,	bus_generic_release_resource),
	DEVMETHOD(bus_activate_resource, admpci_activate_resource),
	DEVMETHOD(bus_deactivate_resource, bus_generic_deactivate_resource),
	DEVMETHOD(bus_setup_intr,	admpci_setup_intr),
	DEVMETHOD(bus_teardown_intr,	admpci_teardown_intr),

	/* pcib interface */
	DEVMETHOD(pcib_maxslots,	admpci_maxslots),
	DEVMETHOD(pcib_read_config,	admpci_read_config),
	DEVMETHOD(pcib_write_config,	admpci_write_config),
	DEVMETHOD(pcib_route_interrupt,	admpci_route_interrupt),

	{0, 0}
};

static driver_t admpci_driver = {
	"pcib",
	admpci_methods,
	sizeof(struct admpci_softc),
};

static devclass_t admpci_devclass;

DRIVER_MODULE(admpci, obio, admpci_driver, admpci_devclass, 0, 0);
