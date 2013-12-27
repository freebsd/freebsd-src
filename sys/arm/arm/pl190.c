/*-
 * Copyright (c) 2012 Oleksandr Tymoshenko <gonzo@bluezbox.com>
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

#ifdef  DEBUG
#define dprintf(fmt, args...) printf(fmt, ##args)
#else
#define dprintf(fmt, args...)
#endif

#define	VICIRQSTATUS	0x000
#define	VICFIQSTATUS	0x004
#define	VICRAWINTR	0x008
#define	VICINTSELECT	0x00C
#define	VICINTENABLE	0x010
#define	VICINTENCLEAR	0x014
#define	VICSOFTINT	0x018
#define	VICSOFTINTCLEAR	0x01C
#define	VICPROTECTION	0x020
#define	VICPERIPHID	0xFE0
#define	VICPRIMECELLID	0xFF0

#define	VIC_NIRQS	32

struct pl190_intc_softc {
	device_t		sc_dev;
	struct resource *	intc_res;
};

static struct pl190_intc_softc *pl190_intc_sc = NULL;

#define	intc_vic_read_4(reg)		\
    bus_read_4(pl190_intc_sc->intc_res, (reg))
#define	intc_vic_write_4(reg, val)		\
    bus_write_4(pl190_intc_sc->intc_res, (reg), (val))

static int
pl190_intc_probe(device_t dev)
{
	if (!ofw_bus_is_compatible(dev, "arm,versatile-vic"))
		return (ENXIO);
	device_set_desc(dev, "ARM PL190 VIC");
	return (BUS_PROBE_DEFAULT);
}

static int
pl190_intc_attach(device_t dev)
{
	struct		pl190_intc_softc *sc = device_get_softc(dev);
	uint32_t	id;
	int		i, rid;

	sc->sc_dev = dev;

	if (pl190_intc_sc)
		return (ENXIO);

	/* Request memory resources */
	rid = 0;
	sc->intc_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	if (sc->intc_res == NULL) {
		device_printf(dev, "Error: could not allocate memory resources\n");
		return (ENXIO);
	}

	pl190_intc_sc = sc;
	/*
	 * All interrupts should use IRQ line
	 */
	intc_vic_write_4(VICINTSELECT, 0x00000000);
	/* Disable all interrupts */
	intc_vic_write_4(VICINTENCLEAR, 0xffffffff);
	/* Enable INT31, SIC IRQ */
	intc_vic_write_4(VICINTENABLE, (1U << 31));

	id = 0;
	for (i = 3; i >= 0; i--) {
		id = (id << 8) | 
		     (intc_vic_read_4(VICPERIPHID + i*4) & 0xff);
	}

	device_printf(dev, "Peripheral ID: %08x\n", id);

	id = 0;
	for (i = 3; i >= 0; i--) {
		id = (id << 8) | 
		     (intc_vic_read_4(VICPRIMECELLID + i*4) & 0xff);
	}

	device_printf(dev, "PrimeCell ID: %08x\n", id);

	return (0);
}

static device_method_t pl190_intc_methods[] = {
	DEVMETHOD(device_probe,		pl190_intc_probe),
	DEVMETHOD(device_attach,	pl190_intc_attach),
	{ 0, 0 }
};

static driver_t pl190_intc_driver = {
	"intc",
	pl190_intc_methods,
	sizeof(struct pl190_intc_softc),
};

static devclass_t pl190_intc_devclass;

DRIVER_MODULE(intc, simplebus, pl190_intc_driver, pl190_intc_devclass, 0, 0);

int
arm_get_next_irq(int last_irq)
{
	uint32_t pending;
	int32_t irq = last_irq + 1;

	/* Sanity check */
	if (irq < 0)
		irq = 0;
	
	pending = intc_vic_read_4(VICIRQSTATUS);
	while (irq < VIC_NIRQS) {
		if (pending & (1 << irq))
			return (irq);
		irq++;
	}

	return (-1);
}

void
arm_mask_irq(uintptr_t nb)
{

	dprintf("%s: %d\n", __func__, nb);
	intc_vic_write_4(VICINTENCLEAR, (1 << nb));
}

void
arm_unmask_irq(uintptr_t nb)
{

	dprintf("%s: %d\n", __func__, nb);
	intc_vic_write_4(VICINTENABLE, (1 << nb));
}
