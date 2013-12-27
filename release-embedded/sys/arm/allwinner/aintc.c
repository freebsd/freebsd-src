/*-
 * Copyright (c) 2012 Ganbold Tsagaankhuu <ganbold@gmail.com>
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/module.h>
#include <sys/rman.h>
#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

/**
 * Interrupt controller registers
 *
 */
#define SW_INT_VECTOR_REG		0x00
#define SW_INT_BASE_ADR_REG		0x04
#define SW_INT_PROTECTION_REG		0x08
#define SW_INT_NMI_CTRL_REG		0x0c

#define SW_INT_IRQ_PENDING_REG0		0x10
#define SW_INT_IRQ_PENDING_REG1		0x14
#define SW_INT_IRQ_PENDING_REG2		0x18

#define SW_INT_FIQ_PENDING_REG0		0x20
#define SW_INT_FIQ_PENDING_REG1		0x24
#define SW_INT_FIQ_PENDING_REG2		0x28

#define SW_INT_SELECT_REG0		0x30
#define SW_INT_SELECT_REG1		0x34
#define SW_INT_SELECT_REG2		0x38

#define SW_INT_ENABLE_REG0		0x40
#define SW_INT_ENABLE_REG1		0x44
#define SW_INT_ENABLE_REG2		0x48

#define SW_INT_MASK_REG0		0x50
#define SW_INT_MASK_REG1		0x54
#define SW_INT_MASK_REG2		0x58

#define SW_INT_IRQNO_ENMI		0

#define SW_INT_IRQ_PENDING_REG(_b)	(0x10 + ((_b) * 4))
#define SW_INT_FIQ_PENDING_REG(_b)	(0x20 + ((_b) * 4))
#define SW_INT_SELECT_REG(_b)		(0x30 + ((_b) * 4))
#define SW_INT_ENABLE_REG(_b)		(0x40 + ((_b) * 4))
#define SW_INT_MASK_REG(_b)		(0x50 + ((_b) * 4))

struct a10_aintc_softc {
	device_t		sc_dev;
	struct resource *	aintc_res;
	bus_space_tag_t		aintc_bst;
	bus_space_handle_t	aintc_bsh;
	uint8_t			ver;
};

static struct a10_aintc_softc *a10_aintc_sc = NULL;

#define	aintc_read_4(reg)	\
	bus_space_read_4(a10_aintc_sc->aintc_bst, a10_aintc_sc->aintc_bsh, reg)
#define	aintc_write_4(reg, val)		\
	bus_space_write_4(a10_aintc_sc->aintc_bst, a10_aintc_sc->aintc_bsh, reg, val)

static int
a10_aintc_probe(device_t dev)
{
	if (!ofw_bus_is_compatible(dev, "allwinner,sun4i-ic"))
		return (ENXIO);
	device_set_desc(dev, "A10 AINTC Interrupt Controller");
	return (BUS_PROBE_DEFAULT);
}

static int
a10_aintc_attach(device_t dev)
{
	struct a10_aintc_softc *sc = device_get_softc(dev);
	int rid = 0;
	int i;
	
	sc->sc_dev = dev;

	if (a10_aintc_sc)
		return (ENXIO);

	sc->aintc_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid, RF_ACTIVE);
	if (!sc->aintc_res) {
		device_printf(dev, "could not allocate resource\n");
		return (ENXIO);
	}

	sc->aintc_bst = rman_get_bustag(sc->aintc_res);
	sc->aintc_bsh = rman_get_bushandle(sc->aintc_res);

	a10_aintc_sc = sc;

	/* Disable & clear all interrupts */
	for (i = 0; i < 3; i++) {
		aintc_write_4(SW_INT_ENABLE_REG(i), 0);
		aintc_write_4(SW_INT_MASK_REG(i), 0xffffffff);
	}
	/* enable protection mode*/
	aintc_write_4(SW_INT_PROTECTION_REG, 0x01);

	/* config the external interrupt source type*/
	aintc_write_4(SW_INT_NMI_CTRL_REG, 0x00);

	return (0);
}

static device_method_t a10_aintc_methods[] = {
	DEVMETHOD(device_probe,		a10_aintc_probe),
	DEVMETHOD(device_attach,	a10_aintc_attach),
	{ 0, 0 }
};

static driver_t a10_aintc_driver = {
	"aintc",
	a10_aintc_methods,
	sizeof(struct a10_aintc_softc),
};

static devclass_t a10_aintc_devclass;

DRIVER_MODULE(aintc, simplebus, a10_aintc_driver, a10_aintc_devclass, 0, 0);

int
arm_get_next_irq(int last_irq)
{
	uint32_t value;
	int i, b;

	for (i = 0; i < 3; i++) {
		value = aintc_read_4(SW_INT_IRQ_PENDING_REG(i));
		for (b = 0; b < 32; b++)
			if (value & (1 << b)) {
				return (i * 32 + b);
			}
	}

	return (-1);
}

void
arm_mask_irq(uintptr_t nb)
{
	uint32_t bit, block, value;
	
	bit = (nb % 32);
	block = (nb / 32);

	value = aintc_read_4(SW_INT_ENABLE_REG(block));
	value &= ~(1 << bit);
	aintc_write_4(SW_INT_ENABLE_REG(block), value);

	value = aintc_read_4(SW_INT_MASK_REG(block));
	value |= (1 << bit);
	aintc_write_4(SW_INT_MASK_REG(block), value);
}

void
arm_unmask_irq(uintptr_t nb)
{
	uint32_t bit, block, value;

	bit = (nb % 32);
	block = (nb / 32);

	value = aintc_read_4(SW_INT_ENABLE_REG(block));
	value |= (1 << bit);
	aintc_write_4(SW_INT_ENABLE_REG(block), value);

	value = aintc_read_4(SW_INT_MASK_REG(block));
	value &= ~(1 << bit);
	aintc_write_4(SW_INT_MASK_REG(block), value);

	if(nb == SW_INT_IRQNO_ENMI) /* must clear pending bit when enabled */
		aintc_write_4(SW_INT_IRQ_PENDING_REG(0), (1 << SW_INT_IRQNO_ENMI));
}
