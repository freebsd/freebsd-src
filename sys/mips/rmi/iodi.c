/*-
 * Copyright (c) 2003-2009 RMI Corporation
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
 * 3. Neither the name of RMI Corporation, nor the names of its contributors,
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
 *
 * RMI_BSD
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#define __RMAN_RESOURCE_VISIBLE
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/reboot.h>
#include <sys/rman.h>
#include <sys/types.h>
#include <sys/malloc.h>
#include <sys/interrupt.h>
#include <sys/module.h>

#include <machine/cpu.h>
#include <machine/bus.h>
#include <machine/intr_machdep.h>
#include <machine/clock.h>	/* for DELAY */
#include <machine/resource.h>

#include <mips/rmi/board.h>
#include <mips/rmi/pic.h>
#include <mips/rmi/interrupt.h>
#include <mips/rmi/msgring.h>
#include <mips/rmi/iomap.h>
#include <mips/rmi/rmi_mips_exts.h>

#include <mips/rmi/dev/xlr/atx_cpld.h>
#include <mips/rmi/dev/xlr/xgmac_mdio.h>

extern bus_space_tag_t uart_bus_space_mem;

static struct resource *
iodi_alloc_resource(device_t, device_t, int, int *,
    rman_res_t, rman_res_t, rman_res_t, u_int);

static int
iodi_activate_resource(device_t, device_t, int, int,
    struct resource *);
static int
iodi_setup_intr(device_t, device_t, struct resource *, int,
    driver_filter_t *, driver_intr_t *, void *, void **);

struct iodi_softc *iodi_softc;	/* There can be only one. */

/*
 * We will manage the Flash/PCMCIA devices in IODI for now.
 * The NOR flash, Compact flash etc. which can be connected on 
 * various chip selects on the peripheral IO, should have a 
 * separate bus later.
 */
static void
bridge_pcmcia_ack(int irq)
{
	xlr_reg_t *mmio = xlr_io_mmio(XLR_IO_FLASH_OFFSET);

	xlr_write_reg(mmio, 0x60, 0xffffffff);
}

static int
iodi_setup_intr(device_t dev, device_t child,
    struct resource *ires, int flags, driver_filter_t *filt,
    driver_intr_t *intr, void *arg, void **cookiep)
{
	const char *name = device_get_name(child);

	if (strcmp(name, "uart") == 0) {
		/* FIXME uart 1? */
		cpu_establish_hardintr("uart", filt, intr, arg,
		    PIC_UART_0_IRQ, flags, cookiep);
		pic_setup_intr(PIC_IRT_UART_0_INDEX, PIC_UART_0_IRQ, 0x1, 1);
	} else if (strcmp(name, "nlge") == 0) {
		int irq;

		/* This is a hack to pass in the irq */
		irq = (intptr_t)ires->__r_i;
		cpu_establish_hardintr("nlge", filt, intr, arg, irq, flags,
		    cookiep);
		pic_setup_intr(irq - PIC_IRQ_BASE, irq, 0x1, 1);
	} else if (strcmp(name, "ehci") == 0) {
		cpu_establish_hardintr("ehci", filt, intr, arg, PIC_USB_IRQ, flags,
		    cookiep);
		pic_setup_intr(PIC_USB_IRQ - PIC_IRQ_BASE, PIC_USB_IRQ, 0x1, 1);
	} else if (strcmp(name, "ata") == 0) {
		xlr_establish_intr("ata", filt, intr, arg, PIC_PCMCIA_IRQ, flags,
		    cookiep, bridge_pcmcia_ack);
		pic_setup_intr(PIC_PCMCIA_IRQ - PIC_IRQ_BASE, PIC_PCMCIA_IRQ, 0x1, 1);
	}
	return (0);
}

static struct resource *
iodi_alloc_resource(device_t bus, device_t child, int type, int *rid,
    rman_res_t start, rman_res_t end, rman_res_t count, u_int flags)
{
	struct resource *res = malloc(sizeof(*res), M_DEVBUF, M_WAITOK);
	const char *name = device_get_name(child);
	int unit;

#ifdef DEBUG
	switch (type) {
	case SYS_RES_IRQ:
		device_printf(bus, "IRQ resource - for %s %jx-%jx\n",
		    device_get_nameunit(child), start, end);
		break;

	case SYS_RES_IOPORT:
		device_printf(bus, "IOPORT resource - for %s %jx-%jx\n",
		    device_get_nameunit(child), start, end);
		break;

	case SYS_RES_MEMORY:
		device_printf(bus, "MEMORY resource - for %s %jx-%jx\n",
		    device_get_nameunit(child), start, end);
		break;
	}
#endif

	if (strcmp(name, "uart") == 0) {
		if ((unit = device_get_unit(child)) == 0) {	/* uart 0 */
			res->r_bushandle = (xlr_io_base + XLR_IO_UART_0_OFFSET);
		} else if (unit == 1) {
			res->r_bushandle = (xlr_io_base + XLR_IO_UART_1_OFFSET);
		} else
			printf("%s: Unknown uart unit\n", __FUNCTION__);

		res->r_bustag = uart_bus_space_mem;
	} else if (strcmp(name, "ehci") == 0) {
		res->r_bushandle = MIPS_PHYS_TO_KSEG1(0x1ef24000);
		res->r_bustag = rmi_pci_bus_space;
	} else if (strcmp(name, "cfi") == 0) {
		res->r_bushandle = MIPS_PHYS_TO_KSEG1(0x1c000000);
		res->r_bustag = 0;
	} else if (strcmp(name, "ata") == 0) {
		res->r_bushandle = MIPS_PHYS_TO_KSEG1(0x1d000000);
		res->r_bustag = rmi_pci_bus_space;  /* byte swapping (not really PCI) */
	}
	/* res->r_start = *rid; */
	return (res);
}

static int
iodi_activate_resource(device_t bus, device_t child, int type, int rid,
    struct resource *r)
{
	return (0);
}

/* prototypes */
static int iodi_probe(device_t);
static int iodi_attach(device_t);
static int iodi_detach(device_t);
static void iodi_identify(driver_t *, device_t);

int
iodi_probe(device_t dev)
{
	return (BUS_PROBE_NOWILDCARD);
}

void
iodi_identify(driver_t * driver, device_t parent)
{

	BUS_ADD_CHILD(parent, 0, "iodi", 0);
}

int
iodi_attach(device_t dev)
{
	device_t tmpd;
	int i;

	/*
	 * Attach each devices
	 */
	device_add_child(dev, "uart", 0);
	device_add_child(dev, "xlr_i2c", 0);
	device_add_child(dev, "xlr_i2c", 1);
	device_add_child(dev, "pcib", 0);
	device_add_child(dev, "rmisec", -1);

	if (xlr_board_info.usb)
		device_add_child(dev, "ehci", 0);

	if (xlr_board_info.cfi)
		device_add_child(dev, "cfi", 0);

	if (xlr_board_info.ata)
		device_add_child(dev, "ata", 0);

	for (i = 0; i < 3; i++) {
		if (xlr_board_info.gmac_block[i].enabled == 0)
			continue;
		tmpd = device_add_child(dev, "nlna", i);
		device_set_ivars(tmpd, &xlr_board_info.gmac_block[i]);
	}

	bus_generic_probe(dev);
	bus_generic_attach(dev);
	return 0;
}

int
iodi_detach(device_t dev)
{
	device_t nlna_dev;
	int error, i, ret;

	error = 0;
	ret = 0;
	for (i = 0; i < 3; i++) {
		nlna_dev = device_find_child(dev, "nlna", i);
		if (nlna_dev != NULL)
			error = bus_generic_detach(nlna_dev);
		if (error)
			ret = error;
	}
	return ret;
}

static device_method_t iodi_methods[] = {
	DEVMETHOD(device_probe, iodi_probe),
	DEVMETHOD(device_attach, iodi_attach),
	DEVMETHOD(device_detach, iodi_detach),
	DEVMETHOD(device_identify, iodi_identify),
	DEVMETHOD(bus_alloc_resource, iodi_alloc_resource),
	DEVMETHOD(bus_activate_resource, iodi_activate_resource),
	DEVMETHOD(bus_add_child, bus_generic_add_child),
	DEVMETHOD(bus_setup_intr, iodi_setup_intr),
	{0, 0},
};

static driver_t iodi_driver = {
	"iodi",
	iodi_methods,
	1			/* no softc */
};
static devclass_t iodi_devclass;

DRIVER_MODULE(iodi, nexus, iodi_driver, iodi_devclass, 0, 0);
