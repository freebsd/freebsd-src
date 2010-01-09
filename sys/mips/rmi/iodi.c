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
 * RMI_BSD */

#define __RMAN_RESOURCE_VISIBLE
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/reboot.h>
#include <sys/types.h>
#include <sys/malloc.h>
#include <sys/bus.h>
#include <sys/interrupt.h>
#include <sys/module.h>

#include <machine/cpu.h>
#include <machine/bus.h>
#include <machine/bus.h>
#include <machine/intr_machdep.h>
#include <mips/rmi/iomap.h>
#include <mips/rmi/pic.h>
#include <mips/rmi/shared_structs.h>
#include <mips/rmi/board.h>
#include <sys/rman.h>


#include <machine/param.h>
#include <machine/intr_machdep.h>
#include <machine/clock.h>	/* for DELAY */
#include <machine/bus.h>
#include <machine/resource.h>
#include <mips/rmi/interrupt.h>
#include <mips/rmi/msgring.h>
#include <mips/rmi/iomap.h>
#include <mips/rmi/debug.h>
#include <mips/rmi/pic.h>
#include <mips/rmi/xlrconfig.h>
#include <mips/rmi/shared_structs.h>
#include <mips/rmi/board.h>

#include <dev/rmi/xlr/atx_cpld.h>
#include <dev/rmi/xlr/xgmac_mdio.h>

extern void iodi_activateirqs(void);

extern bus_space_tag_t uart_bus_space_mem;

static struct resource *
iodi_alloc_resource(device_t, device_t, int, int *,
    u_long, u_long, u_long, u_int);

static int
iodi_activate_resource(device_t, device_t, int, int,
    struct resource *);
static int
iodi_setup_intr(device_t, device_t, struct resource *, int,
    driver_filter_t *, driver_intr_t *, void *, void **);

struct iodi_softc *iodi_softc;	/* There can be only one. */

/*
static void pic_usb_ack(void *arg)
{
	xlr_reg_t *mmio = xlr_io_mmio(XLR_IO_PIC_OFFSET);
	int irq = PIC_USB_IRQ ;

	mtx_lock_spin(&xlr_pic_lock);
	xlr_write_reg(mmio, PIC_INT_ACK, (1 << (irq - PIC_IRQ_BASE)));
	mtx_unlock_spin(&xlr_pic_lock);
}
*/


static int
iodi_setup_intr(device_t dev, device_t child,
    struct resource *ires, int flags, driver_filter_t * filt, driver_intr_t * intr, void *arg,
    void **cookiep)
{
	int level;
	xlr_reg_t *mmio = xlr_io_mmio(XLR_IO_PIC_OFFSET);
	xlr_reg_t reg;

	/* FIXME is this the right place to fiddle with PIC? */
	if (strcmp(device_get_name(child), "uart") == 0) {
		/* FIXME uart 1? */
		if (rmi_spin_mutex_safe)
			mtx_lock_spin(&xlr_pic_lock);
		level = PIC_IRQ_IS_EDGE_TRIGGERED(PIC_IRT_UART_0_INDEX);
		xlr_write_reg(mmio, PIC_IRT_0_UART_0, 0x01);
		xlr_write_reg(mmio, PIC_IRT_1_UART_0, ((1 << 31) | (level << 30) | (1 << 6) | (PIC_UART_0_IRQ)));
		if (rmi_spin_mutex_safe)
			mtx_unlock_spin(&xlr_pic_lock);
		cpu_establish_hardintr("uart", filt,
		    (driver_intr_t *) intr, (void *)arg, PIC_UART_0_IRQ, flags, cookiep);

	} else if (strcmp(device_get_name(child), "rge") == 0) {
		int irq;

		/* This is a hack to pass in the irq */
		irq = (int)ires->__r_i;
		if (rmi_spin_mutex_safe)
			mtx_lock_spin(&xlr_pic_lock);
		reg = xlr_read_reg(mmio, PIC_IRT_1_BASE + irq - PIC_IRQ_BASE);
		xlr_write_reg(mmio, PIC_IRT_1_BASE + irq - PIC_IRQ_BASE, reg | (1 << 6) | (1 << 30) | (1 << 31));
		if (rmi_spin_mutex_safe)
			mtx_unlock_spin(&xlr_pic_lock);
		cpu_establish_hardintr("rge", filt, (driver_intr_t *) intr, (void *)arg, irq, flags, cookiep);

	} else if (strcmp(device_get_name(child), "ehci") == 0) {
		if (rmi_spin_mutex_safe)
			mtx_lock_spin(&xlr_pic_lock);
		reg = xlr_read_reg(mmio, PIC_IRT_1_BASE + PIC_USB_IRQ - PIC_IRQ_BASE);
		xlr_write_reg(mmio, PIC_IRT_1_BASE + PIC_USB_IRQ - PIC_IRQ_BASE, reg | (1 << 6) | (1 << 30) | (1 << 31));
		if (rmi_spin_mutex_safe)
			mtx_unlock_spin(&xlr_pic_lock);
		cpu_establish_hardintr("ehci", filt, (driver_intr_t *) intr, (void *)arg, PIC_USB_IRQ, flags, cookiep);
	}
	/*
	 * This causes a panic and looks recursive to me (RRS).
	 * BUS_SETUP_INTR(device_get_parent(dev), child, ires, flags, filt,
	 * intr, arg, cookiep);
	 */

	return (0);
}

/* Strange hook found in mips/include/bus.h */
#ifndef MIPS_BUS_SPACE_PCI
#define MIPS_BUS_SPACE_PCI	10
#endif

static struct resource *
iodi_alloc_resource(device_t bus, device_t child, int type, int *rid,
    u_long start, u_long end, u_long count, u_int flags)
{
	struct resource *res = malloc(sizeof(*res), M_DEVBUF, M_WAITOK);
	int unit;

#ifdef DEBUG
	switch (type) {
	case SYS_RES_IRQ:
		device_printf(bus, "IRQ resource - for %s %lx-%lx\n",
		    device_get_nameunit(child), start, end);
		break;

	case SYS_RES_IOPORT:
		device_printf(bus, "IOPORT resource - for %s %lx-%lx\n",
		    device_get_nameunit(child), start, end);
		break;

	case SYS_RES_MEMORY:
		device_printf(bus, "MEMORY resource - for %s %lx-%lx\n",
		    device_get_nameunit(child), start, end);
		break;
	}
#endif

	if (strcmp(device_get_name(child), "uart") == 0) {
		if ((unit = device_get_unit(child)) == 0) {	/* uart 0 */
			res->r_bushandle = (xlr_io_base + XLR_IO_UART_0_OFFSET);
		} else if (unit == 1) {
			res->r_bushandle = (xlr_io_base + XLR_IO_UART_1_OFFSET);
		} else
			printf("%s: Unknown uart unit\n", __FUNCTION__);

		res->r_bustag = uart_bus_space_mem;
	} else if (strcmp(device_get_name(child), "ehci") == 0) {
		res->r_bushandle = 0xbef24000;
		res->r_bustag = (bus_space_tag_t) MIPS_BUS_SPACE_PCI;
	} else if (strcmp(device_get_name(child), "cfi") == 0) {
		res->r_bushandle = 0xbc000000;
		res->r_bustag = 0;
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
static void iodi_identify(driver_t *, device_t);

int
iodi_probe(device_t dev)
{
	return 0;
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

	/*
	 * Attach each devices
	 */
	device_add_child(dev, "uart", 0);
	device_add_child(dev, "xlr_i2c", 0);

	if (xlr_board_info.usb)
		device_add_child(dev, "ehci", 0);

	if (xlr_board_info.cfi)
		device_add_child(dev, "cfi", 0);

	if (xlr_board_info.gmac_block[0].enabled) {
		tmpd = device_add_child(dev, "rge", 0);
		device_set_ivars(tmpd, &xlr_board_info.gmac_block[0]);

		tmpd = device_add_child(dev, "rge", 1);
		device_set_ivars(tmpd, &xlr_board_info.gmac_block[0]);

		tmpd = device_add_child(dev, "rge", 2);
		device_set_ivars(tmpd, &xlr_board_info.gmac_block[0]);

		tmpd = device_add_child(dev, "rge", 3);
		device_set_ivars(tmpd, &xlr_board_info.gmac_block[0]);
	}
	if (xlr_board_info.gmac_block[1].enabled) {
		if (xlr_board_info.gmac_block[1].type == XLR_GMAC) {
			tmpd = device_add_child(dev, "rge", 4);
			device_set_ivars(tmpd, &xlr_board_info.gmac_block[1]);

			tmpd = device_add_child(dev, "rge", 5);
			device_set_ivars(tmpd, &xlr_board_info.gmac_block[1]);

			tmpd = device_add_child(dev, "rge", 6);
			device_set_ivars(tmpd, &xlr_board_info.gmac_block[1]);

			tmpd = device_add_child(dev, "rge", 7);
			device_set_ivars(tmpd, &xlr_board_info.gmac_block[1]);
		} else if (xlr_board_info.gmac_block[1].type == XLR_XGMAC) {
#if 0				/* XGMAC not yet */
			tmpd = device_add_child(dev, "rge", 4);
			device_set_ivars(tmpd, &xlr_board_info.gmac_block[1]);

			tmpd = device_add_child(dev, "rge", 5);
			device_set_ivars(tmpd, &xlr_board_info.gmac_block[1]);
#endif
		} else
			device_printf(dev, "Unknown type of gmac 1\n");
	}
	bus_generic_probe(dev);
	bus_generic_attach(dev);
	return 0;
}

static device_method_t iodi_methods[] = {
	DEVMETHOD(device_probe, iodi_probe),
	DEVMETHOD(device_attach, iodi_attach),
	DEVMETHOD(device_identify, iodi_identify),
	DEVMETHOD(bus_alloc_resource, iodi_alloc_resource),
	DEVMETHOD(bus_activate_resource, iodi_activate_resource),
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
