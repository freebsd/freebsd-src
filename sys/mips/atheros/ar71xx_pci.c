/*-
 * Copyright (c) 2009, Oleksandr Tymoshenko <gonzo@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
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

#include "mips/atheros/ar71xxreg.h"

#undef AR71XX_PCI_DEBUG
#ifdef AR71XX_PCI_DEBUG
#define dprintf printf
#else
#define dprintf(x, arg...)
#endif

struct ar71xx_pci_softc {
	device_t		sc_dev;

	int			sc_busno;
	struct rman		sc_mem_rman;
	struct rman		sc_irq_rman;

	struct resource		*sc_irq;
	void			*sc_ih;
};

/* 
 * get bitmask for bytes of interest: 
 *   0 - we want this byte, 1 - ignore it. e.g: we read 1 byte 
 *   from register 7. Bitmask would be: 0111
 */
static uint32_t
ar71xx_get_bytes_to_read(int reg, int bytes)
{
	uint32_t bytes_to_read = 0;
	if ((bytes % 4) == 0)
		bytes_to_read = 0;
	else if ((bytes % 4) == 1)
		bytes_to_read = (~(1 << (reg % 4))) & 0xf;
	else if ((bytes % 4) == 2)
		bytes_to_read = (~(3 << (reg % 4))) & 0xf;
	else
		panic("%s: wrong combination", __func__);

	return (bytes_to_read);
}

static int 
ar71xx_pci_check_bus_error(void)
{
	uint32_t error, addr, has_errors = 0;
	error = ATH_READ_REG(AR71XX_PCI_ERROR) & 0x3;
	dprintf("%s: PCI error = %02x\n", __func__, error);
	if (error) {
		addr = ATH_READ_REG(AR71XX_PCI_ERROR_ADDR);

		/* Do not report it yet */
#if 0
		printf("PCI bus error %d at addr 0x%08x\n", error, addr);
#endif
		ATH_WRITE_REG(AR71XX_PCI_ERROR, error);
		has_errors = 1;
	}

	error = ATH_READ_REG(AR71XX_PCI_AHB_ERROR) & 0x1;
	dprintf("%s: AHB error = %02x\n", __func__, error);
	if (error) {
		addr = ATH_READ_REG(AR71XX_PCI_AHB_ERROR_ADDR);
		/* Do not report it yet */
#if 0
		printf("AHB bus error %d at addr 0x%08x\n", error, addr);
#endif
		ATH_WRITE_REG(AR71XX_PCI_AHB_ERROR, error);
		has_errors = 1;
	}

	return (has_errors);
}

static uint32_t
ar71xx_pci_make_addr(int bus, int slot, int func, int reg)
{
	if (bus == 0) {
		return ((1 << slot) | (func << 8) | (reg & ~3));
	} else {
		return ((bus << 16) | (slot << 11) | (func << 8) 
		    | (reg  & ~3) | 1);
	}
}

static int
ar71xx_pci_conf_setup(int bus, int slot, int func, int reg, int bytes, 
    uint32_t cmd)
{
	uint32_t addr = ar71xx_pci_make_addr(bus, slot, func, (reg & ~3));
	cmd |= (ar71xx_get_bytes_to_read(reg, bytes) << 4);
	
	ATH_WRITE_REG(AR71XX_PCI_CONF_ADDR, addr);
	ATH_WRITE_REG(AR71XX_PCI_CONF_CMD, cmd);

	dprintf("%s: tag (%x, %x, %x) %d/%d addr=%08x, cmd=%08x\n", __func__, 
	    bus, slot, func, reg, bytes, addr, cmd);

	return ar71xx_pci_check_bus_error();
}

static uint32_t
ar71xx_pci_read_config(device_t dev, int bus, int slot, int func, int reg,
    int bytes)
{
	uint32_t data;
	uint32_t cmd, shift, mask;

	/* register access is 32-bit aligned */
	shift = (reg & 3) * 8;
	if (shift)
		mask = (1 << shift) - 1;
	else
		mask = 0xffffffff;

	dprintf("%s: tag (%x, %x, %x) reg %d(%d)\n", __func__, bus, slot, 
	    func, reg, bytes);

	if ((bus == 0) && (slot == 0) && (func == 0)) {
		cmd = PCI_LCONF_CMD_READ | (reg & ~3);
		ATH_WRITE_REG(AR71XX_PCI_LCONF_CMD, cmd);
		data = ATH_READ_REG(AR71XX_PCI_LCONF_READ_DATA);
	} else {
		 if (ar71xx_pci_conf_setup(bus, slot, func, reg, bytes, 
		     PCI_CONF_CMD_READ) == 0)
			 data = ATH_READ_REG(AR71XX_PCI_CONF_READ_DATA);
		 else
			 data = -1;
	}

	/* get request bytes from 32-bit word */
	data = (data >> shift) & mask;

 	dprintf("%s: read 0x%x\n", __func__, data);

	return (data);
}

static void
ar71xx_pci_write_config(device_t dev, int bus, int slot, int func, int reg,
    uint32_t data, int bytes)
{
	uint32_t cmd;

	dprintf("%s: tag (%x, %x, %x) reg %d(%d)\n", __func__, bus, slot, 
	    func, reg, bytes);

	data = data << (8*(reg % 4));

	if ((bus == 0) && (slot == 0) && (func == 0)) {
		cmd = PCI_LCONF_CMD_WRITE | (reg & ~3);
		cmd |= ar71xx_get_bytes_to_read(reg, bytes) << 20;
		ATH_WRITE_REG(AR71XX_PCI_LCONF_CMD, cmd);
		ATH_WRITE_REG(AR71XX_PCI_LCONF_WRITE_DATA, data);
	} else {
		 if (ar71xx_pci_conf_setup(bus, slot, func, reg, bytes, 
		     PCI_CONF_CMD_WRITE) == 0)
			 ATH_WRITE_REG(AR71XX_PCI_CONF_WRITE_DATA, data);
	}
}

static int
at71xx_pci_intr(void *v)
{
	panic("Implement me: %s\n", __func__);
	return FILTER_HANDLED;
}

static int
ar71xx_pci_probe(device_t dev)
{

	return (0);
}

static int
ar71xx_pci_attach(device_t dev)
{
	int busno = 0;
	int rid = 0;
	uint32_t reset;
	struct ar71xx_pci_softc *sc = device_get_softc(dev);

	sc->sc_mem_rman.rm_type = RMAN_ARRAY;
	sc->sc_mem_rman.rm_descr = "ar71xx PCI memory window";
	if (rman_init(&sc->sc_mem_rman) != 0 || 
	    rman_manage_region(&sc->sc_mem_rman, AR71XX_PCI_MEM_BASE, 
		AR71XX_PCI_MEM_BASE + AR71XX_PCI_MEM_SIZE - 1) != 0) {
		panic("ar71xx_pci_attach: failed to set up I/O rman");
	}

	sc->sc_irq_rman.rm_type = RMAN_ARRAY;
	sc->sc_irq_rman.rm_descr = "ar71xx PCI IRQs";
	if (rman_init(&sc->sc_irq_rman) != 0 ||
	    rman_manage_region(&sc->sc_irq_rman, AR71XX_PCI_IRQ_START, 
	        AR71XX_PCI_IRQ_END) != 0)
		panic("ar71xx_pci_attach: failed to set up IRQ rman");


	ATH_WRITE_REG(AR71XX_PCI_INTR_STATUS, 0);
	ATH_WRITE_REG(AR71XX_PCI_INTR_MASK, 0);

	/* Hook up our interrupt handler. */
	if ((sc->sc_irq = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
	    RF_SHAREABLE | RF_ACTIVE)) == NULL) {
		device_printf(dev, "unable to allocate IRQ resource\n");
		return ENXIO;
	}

	if ((bus_setup_intr(dev, sc->sc_irq, INTR_TYPE_MISC,
			    at71xx_pci_intr, NULL, sc, &sc->sc_ih))) {
		device_printf(dev, 
		    "WARNING: unable to register interrupt handler\n");
		return ENXIO;
	}

	/* reset PCI core and PCI bus */
	reset = ATH_READ_REG(AR71XX_RST_RESET);
	reset |= (RST_RESET_PCI_CORE | RST_RESET_PCI_BUS);
	ATH_WRITE_REG(AR71XX_RST_RESET, reset);
	DELAY(1000);

	reset &= ~(RST_RESET_PCI_CORE | RST_RESET_PCI_BUS);
	ATH_WRITE_REG(AR71XX_RST_RESET, reset);
	DELAY(1000);

	/* Init PCI windows */
	ATH_WRITE_REG(AR71XX_PCI_WINDOW0, PCI_WINDOW0_ADDR);
	ATH_WRITE_REG(AR71XX_PCI_WINDOW1, PCI_WINDOW1_ADDR);
	ATH_WRITE_REG(AR71XX_PCI_WINDOW2, PCI_WINDOW2_ADDR);
	ATH_WRITE_REG(AR71XX_PCI_WINDOW3, PCI_WINDOW3_ADDR);
	ATH_WRITE_REG(AR71XX_PCI_WINDOW4, PCI_WINDOW4_ADDR);
	ATH_WRITE_REG(AR71XX_PCI_WINDOW5, PCI_WINDOW5_ADDR);
	ATH_WRITE_REG(AR71XX_PCI_WINDOW6, PCI_WINDOW6_ADDR);
	ATH_WRITE_REG(AR71XX_PCI_WINDOW7, PCI_WINDOW7_CONF_ADDR);
	DELAY(1000);

	ar71xx_pci_check_bus_error();

	/* Fixup internal PCI bridge */
	ar71xx_pci_write_config(dev, 0, 0, 0, PCIR_COMMAND, 
            PCIM_CMD_BUSMASTEREN | PCIM_CMD_MEMEN 
	    | PCIM_CMD_SERRESPEN | PCIM_CMD_BACKTOBACK
	    | PCIM_CMD_PERRESPEN | PCIM_CMD_MWRICEN, 2);

	device_add_child(dev, "pci", busno);
	return (bus_generic_attach(dev));
}

static int
ar71xx_pci_read_ivar(device_t dev, device_t child, int which, uintptr_t *result)
{
	struct ar71xx_pci_softc *sc = device_get_softc(dev);

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
ar71xx_pci_write_ivar(device_t dev, device_t child, int which, uintptr_t result)
{
	struct ar71xx_pci_softc * sc = device_get_softc(dev);

	switch (which) {
	case PCIB_IVAR_BUS:
		sc->sc_busno = result;
		return (0);
	}

	return (ENOENT);
}

static struct resource *
ar71xx_pci_alloc_resource(device_t bus, device_t child, int type, int *rid,
    u_long start, u_long end, u_long count, u_int flags)
{

	struct ar71xx_pci_softc *sc = device_get_softc(bus);	
	struct resource *rv = NULL;
	struct rman *rm;

	switch (type) {
	case SYS_RES_IRQ:
		rm = &sc->sc_irq_rman;
		break;
	case SYS_RES_MEMORY:
		rm = &sc->sc_mem_rman;
		break;
	default:
		return (NULL);
	}

	rv = rman_reserve_resource(rm, start, end, count, flags, child);

	if (rv == NULL)
		return (NULL);

	rman_set_rid(rv, *rid);

	if (flags & RF_ACTIVE) {
		if (bus_activate_resource(child, type, *rid, rv)) {
			rman_release_resource(rv);
			return (NULL);
		}
	} 

	return (rv);
}

static int
ar71xx_pci_teardown_intr(device_t dev, device_t child, struct resource *res,
    void *cookie)
{

	return (intr_event_remove_handler(cookie));
}

static int
ar71xx_pci_maxslots(device_t dev)
{

	return (PCI_SLOTMAX);
}

static int
ar71xx_pci_route_interrupt(device_t pcib, device_t device, int pin)
{

	return (pin);
}

static device_method_t ar71xx_pci_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		ar71xx_pci_probe),
	DEVMETHOD(device_attach,	ar71xx_pci_attach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	bus_generic_resume),

	/* Bus interface */
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_read_ivar,	ar71xx_pci_read_ivar),
	DEVMETHOD(bus_write_ivar,	ar71xx_pci_write_ivar),
	DEVMETHOD(bus_alloc_resource,	ar71xx_pci_alloc_resource),
	DEVMETHOD(bus_release_resource,	bus_generic_release_resource),
	DEVMETHOD(bus_activate_resource, bus_generic_activate_resource),
	DEVMETHOD(bus_deactivate_resource, bus_generic_deactivate_resource),
	DEVMETHOD(bus_setup_intr,	bus_generic_setup_intr),
	DEVMETHOD(bus_teardown_intr,	ar71xx_pci_teardown_intr),

	/* pcib interface */
	DEVMETHOD(pcib_maxslots,	ar71xx_pci_maxslots),
	DEVMETHOD(pcib_read_config,	ar71xx_pci_read_config),
	DEVMETHOD(pcib_write_config,	ar71xx_pci_write_config),
	DEVMETHOD(pcib_route_interrupt,	ar71xx_pci_route_interrupt),

	{0, 0}
};

static driver_t ar71xx_pci_driver = {
	"pcib",
	ar71xx_pci_methods,
	sizeof(struct ar71xx_pci_softc),
};

static devclass_t ar71xx_pci_devclass;

DRIVER_MODULE(ar71xx_pci, nexus, ar71xx_pci_driver, ar71xx_pci_devclass, 0, 0);
