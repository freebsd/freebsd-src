/*-
 * Copyright (c) 1998 Doug Rabson
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
 *
 *	$Id: isa.c,v 1.1 1998/07/22 08:29:26 dfr Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/malloc.h>

#include <isa/isareg.h>
#include <isa/isavar.h>
#include <machine/intr.h>

/*
 * The structure used to attach devices to the Isa.
 */
struct isa_device {
	int		id_port;
	int		id_portsize;
	int		id_flags;
	int		id_irq;
};

#define DEVTOISA(dev)	((struct isa_device*) device_get_ivars(dev))

static devclass_t isa_devclass;

/*
 * Device methods
 */
static int isa_probe(device_t dev);
static int isa_attach(device_t dev);
static void isa_print_child(device_t dev, device_t child);
static int isa_read_ivar(device_t dev, device_t child, int which, u_long *result);
static int isa_write_ivar(device_t dev, device_t child, int which, u_long result);
static void *isa_create_intr(device_t dev, device_t child, int irq,
			     driver_intr_t *intr, void *arg);
static int isa_connect_intr(device_t dev, void *ih);

static device_method_t isa_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		isa_probe),
	DEVMETHOD(device_attach,	isa_attach),
	DEVMETHOD(device_detach,	bus_generic_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),

	/* Bus interface */
	DEVMETHOD(bus_print_child,	isa_print_child),
	DEVMETHOD(bus_read_ivar,	isa_read_ivar),
	DEVMETHOD(bus_write_ivar,	isa_write_ivar),
	DEVMETHOD(bus_create_intr,	isa_create_intr),
	DEVMETHOD(bus_connect_intr,	isa_connect_intr),

	{ 0, 0 }
};

static driver_t isa_driver = {
	"isa",
	isa_methods,
	DRIVER_TYPE_MISC,
	1,			/* no softc */
};

static void
isa_add_device(device_t dev, const char *name, int unit)
{
	struct isa_device *idev;
	device_t child;
	int t;

	idev = malloc(sizeof(struct isa_device), M_DEVBUF, M_NOWAIT);
	if (!idev)
		return;

	if (resource_int_value(name, unit, "port", &t) == 0)
		idev->id_port = t;
	else
		idev->id_port = 0;
	if (resource_int_value(name, unit, "portsize", &t) == 0)
		idev->id_portsize = t;
	else
		idev->id_portsize = 0;
	if (resource_int_value(name, unit, "flags", &t) == 0)
		idev->id_flags = t;
	else
		idev->id_flags = 0;
	if (resource_int_value(name, unit, "irq", &t) == 0)
		idev->id_irq = t;
	else
		idev->id_irq = -1;

	child = device_add_child(dev, name, unit, idev);
	if (!child)
		return;

	if (resource_int_value(name, unit, "disabled", &t) == 0 && t != 0)
		device_disable(child);
}

static void
isa_intr_enable(int irq)
{
	int s = splhigh();
	if (irq < 8)
		outb(IO_ICU1+1, inb(IO_ICU1+1) & ~(1 << irq));
	else
		outb(IO_ICU2+1, inb(IO_ICU2+1) & ~(1 << irq));
	splx(s);
}

static void
isa_intr_disable(int irq)
{
	int s = splhigh();
	if (irq < 8)
		outb(IO_ICU1+1, inb(IO_ICU1+1) | (1 << irq));
	else
		outb(IO_ICU2+1, inb(IO_ICU2+1) | (1 << irq));
	splx(s);
}

int
isa_irq_pending(void)
{
	u_char irr1;
	u_char irr2;

	irr1 = inb(IO_ICU1);
	irr2 = inb(IO_ICU2);
	return ((irr2 << 8) | irr1);
}

int
isa_irq_mask(void)
{
	u_char irr1;
	u_char irr2;

	irr1 = inb(IO_ICU1+1);
	irr2 = inb(IO_ICU2+1);
	return ((irr2 << 8) | irr1);
}

/*
 * At 'probe' time, we add all the devices which we know about to the
 * bus.  The generic attach routine will probe and attach them if they
 * are alive.
 */
static int
isa_probe(device_t dev)
{
	int i;

	/*
	 * Add all devices configured to be attached to isa0.
	 */
	for (i = resource_query_string(-1, "at", "isa0");
	     i != -1;
	     i = resource_query_string(i, "at", "isa0")) {
		isa_add_device(dev, resource_query_name(i),
			       resource_query_unit(i));
	}

	return 0;
}

extern device_t isa_bus_device;

static int
isa_attach(device_t dev)
{
	/* make sure chaining irq is enabled */
	isa_intr_enable(2);

	/*
	 * Arrange for bus_generic_attach(dev) to be called later.
	 */
	isa_bus_device = dev;
	return 0;
}

static void
isa_print_child(device_t bus, device_t dev)
{
	struct isa_device* idev = DEVTOISA(dev);

	printf(" at");
	if (idev->id_port)
		printf(" 0x%x", idev->id_port);
	if (idev->id_portsize > 0)
		printf("-0x%x", idev->id_port + idev->id_portsize - 1);
	if (idev->id_irq >= 0)
		printf(" irq %d", idev->id_irq);
	printf(" on %s%d",
	       device_get_name(bus), device_get_unit(bus));
}

static int
isa_read_ivar(device_t bus, device_t dev,
	       int index, u_long* result)
{
	struct isa_device* idev = DEVTOISA(dev);

	switch (index) {
	case ISA_IVAR_PORT:
		*result = idev->id_port;
		break;
	case ISA_IVAR_PORTSIZE:
		*result = idev->id_portsize;
		break;
	case ISA_IVAR_FLAGS:
		*result = idev->id_flags;
		break;
	case ISA_IVAR_IRQ:
		*result = idev->id_irq;
		break;
	}
	return ENOENT;
}

static int
isa_write_ivar(device_t bus, device_t dev,
	       int index, u_long value)
{
	struct isa_device* idev = DEVTOISA(dev);

	switch (index) {
	case ISA_IVAR_PORT:
		idev->id_port = value;
		break;
	case ISA_IVAR_PORTSIZE:
		idev->id_portsize = value;
		break;
	case ISA_IVAR_FLAGS:
		idev->id_flags = value;
		break;
	case ISA_IVAR_IRQ:
		idev->id_irq = value;
		break;
	}
	return ENOENT;
}

struct isa_intr {
	void *ih;
	driver_intr_t *intr;
	void *arg;
	int irq;
};

static void
isa_handle_intr(void *arg)
{
	struct isa_intr *ii = arg;
	int irq = ii->irq;

	ii->intr(ii->arg);

	if (ii->irq > 7)
		outb(IO_ICU2, 0x20 | (irq & 7));
	outb(IO_ICU1, 0x20 | (irq > 7 ? 2 : irq));
}

static void *
isa_create_intr(device_t dev, device_t child, int irq,
		driver_intr_t *intr, void *arg)
{
	struct isa_intr *ii;

	if (irq == 2) irq = 9;
	ii = malloc(sizeof(struct isa_intr), M_DEVBUF, M_NOWAIT);
	if (!ii)
		return NULL;
	ii->intr = intr;
	ii->arg = arg;
	ii->irq = irq;
	ii->ih = BUS_CREATE_INTR(device_get_parent(dev), dev,
				  0x800 + (irq << 4),
				  isa_handle_intr, ii);
	if (!ii->ih) {
		free(ii, M_DEVBUF);
		return NULL;
	}

	return ii;
}

static int
isa_connect_intr(device_t dev, void *ih)
{
	struct isa_intr *ii = ih;
	struct alpha_intr *i = ii->ih;

	isa_intr_enable(ii->irq);
	return BUS_CONNECT_INTR(device_get_parent(dev), ii->ih);
}

DRIVER_MODULE(isa, cia, isa_driver, isa_devclass, 0, 0);
