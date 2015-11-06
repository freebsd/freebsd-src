/*
 * Copyright 2015 Andrew Turner.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/rman.h>

#include <machine/bus.h>
#include <machine/resource.h>

#include <dev/ofw/ofw_bus_subr.h>
#include <dev/ofw/ofw_bus.h>

#include <arm/broadcom/bcm2835/bcm2836.h>

#define	ARM_LOCAL_BASE	0x40000000
#define	ARM_LOCAL_SIZE	0x00001000

#define	ARM_LOCAL_CONTROL		0x00
#define	ARM_LOCAL_PRESCALER		0x08
#define	 PRESCALER_19_2			0x80000000 /* 19.2 MHz */
#define	ARM_LOCAL_INT_TIMER(n)		(0x40 + (n) * 4)
#define	ARM_LOCAL_INT_MAILBOX(n)	(0x50 + (n) * 4)
#define	ARM_LOCAL_INT_PENDING(n)	(0x60 + (n) * 4)
#define	 INT_PENDING_MASK		0x011f
#define	MAILBOX0_IRQ			4
#define	MAILBOX0_IRQEN			(1 << 0)

/*
 * A driver for features of the bcm2836.
 */

struct bcm2836_softc {
	device_t	 sc_dev;
	struct resource *sc_mem;
};

static device_identify_t bcm2836_identify;
static device_probe_t bcm2836_probe;
static device_attach_t bcm2836_attach;

struct bcm2836_softc *softc;

static void
bcm2836_identify(driver_t *driver, device_t parent)
{

	if (BUS_ADD_CHILD(parent, 0, "bcm2836", -1) == NULL)
		device_printf(parent, "add child failed\n");
}

static int
bcm2836_probe(device_t dev)
{

	if (softc != NULL)
		return (ENXIO);

	device_set_desc(dev, "Broadcom bcm2836");

	return (BUS_PROBE_DEFAULT);
}

static int
bcm2836_attach(device_t dev)
{
	int i, rid;

	softc = device_get_softc(dev);
	softc->sc_dev = dev;

	rid = 0;
	softc->sc_mem = bus_alloc_resource(dev, SYS_RES_MEMORY, &rid,
	    ARM_LOCAL_BASE, ARM_LOCAL_BASE + ARM_LOCAL_SIZE, ARM_LOCAL_SIZE,
	    RF_ACTIVE);
	if (softc->sc_mem == NULL) {
		device_printf(dev, "could not allocate memory resource\n");
		return (ENXIO);
	}

	bus_write_4(softc->sc_mem, ARM_LOCAL_CONTROL, 0);
	bus_write_4(softc->sc_mem, ARM_LOCAL_PRESCALER, PRESCALER_19_2);

	for (i = 0; i < 4; i++)
		bus_write_4(softc->sc_mem, ARM_LOCAL_INT_TIMER(i), 0);

	for (i = 0; i < 4; i++)
		bus_write_4(softc->sc_mem, ARM_LOCAL_INT_MAILBOX(i), 1);

	return (0);
}

int
bcm2836_get_next_irq(int last_irq)
{
	uint32_t reg;
	int cpu;
	int irq;

	cpu = PCPU_GET(cpuid);

	reg = bus_read_4(softc->sc_mem, ARM_LOCAL_INT_PENDING(cpu));
	reg &= INT_PENDING_MASK;
	if (reg == 0)
		return (-1);

	irq = ffs(reg) - 1;

	return (irq);
}

void
bcm2836_mask_irq(uintptr_t irq)
{
	uint32_t reg;
#ifdef SMP
	int cpu;
#endif
	int i;

	if (irq < MAILBOX0_IRQ) {
		for (i = 0; i < 4; i++) {
			reg = bus_read_4(softc->sc_mem,
			    ARM_LOCAL_INT_TIMER(i));
			reg &= ~(1 << irq);
			bus_write_4(softc->sc_mem,
			    ARM_LOCAL_INT_TIMER(i), reg);
		}
#ifdef SMP
	} else if (irq == MAILBOX0_IRQ) {
		/* Mailbox 0 for IPI */
		cpu = PCPU_GET(cpuid);
		reg = bus_read_4(softc->sc_mem, ARM_LOCAL_INT_MAILBOX(cpu));
		reg &= ~MAILBOX0_IRQEN;
		bus_write_4(softc->sc_mem, ARM_LOCAL_INT_MAILBOX(cpu), reg);
#endif
	}
}

void
bcm2836_unmask_irq(uintptr_t irq)
{
	uint32_t reg;
#ifdef SMP
	int cpu;
#endif
	int i;

	if (irq < MAILBOX0_IRQ) {
		for (i = 0; i < 4; i++) {
			reg = bus_read_4(softc->sc_mem,
			    ARM_LOCAL_INT_TIMER(i));
			reg |= (1 << irq);
			bus_write_4(softc->sc_mem,
			    ARM_LOCAL_INT_TIMER(i), reg);
		}
#ifdef SMP
	} else if (irq == MAILBOX0_IRQ) {
		/* Mailbox 0 for IPI */
		cpu = PCPU_GET(cpuid);
		reg = bus_read_4(softc->sc_mem, ARM_LOCAL_INT_MAILBOX(cpu));
		reg |= MAILBOX0_IRQEN;
		bus_write_4(softc->sc_mem, ARM_LOCAL_INT_MAILBOX(cpu), reg);
#endif
	}
}

static device_method_t bcm2836_methods[] = {
	/* Device interface */
	DEVMETHOD(device_identify,	bcm2836_identify),
	DEVMETHOD(device_probe,		bcm2836_probe),
	DEVMETHOD(device_attach,	bcm2836_attach),

	DEVMETHOD_END
};

static devclass_t bcm2836_devclass;

static driver_t bcm2836_driver = {
	"bcm2836",
	bcm2836_methods,
	sizeof(struct bcm2836_softc),
};

EARLY_DRIVER_MODULE(bcm2836, nexus, bcm2836_driver, bcm2836_devclass, 0, 0,
    BUS_PASS_INTERRUPT + BUS_PASS_ORDER_MIDDLE);
