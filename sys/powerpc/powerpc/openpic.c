/*
 * Copyright (C) 2002 Benno Rice.
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
 * THIS SOFTWARE IS PROVIDED BY Benno Rice ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/kernel.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_pci.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>

#include <machine/bus.h>
#include <machine/intr.h>
#include <machine/intr_machdep.h>
#include <machine/md_var.h>
#include <machine/nexusvar.h>
#include <machine/pio.h>
#include <machine/resource.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <sys/rman.h>

#include <machine/openpicreg.h>
#include <machine/openpicvar.h>

#include "pic_if.h"

/*
 * Device interface.
 */
static int		openpic_probe(device_t);
static int		openpic_attach(device_t);

/*
 * PIC interface.
 */
static struct resource	*openpic_allocate_intr(device_t, device_t, int *,
			    u_long, u_int);
static int		openpic_setup_intr(device_t, device_t,
			    struct resource *, int, driver_intr_t, void *,
			    void **);
static int		openpic_teardown_intr(device_t, device_t,
			    struct resource *, void *);
static int		openpic_release_intr(device_t dev, device_t, int,
			    struct resource *res);

/*
 * Local routines
 */
static u_int		openpic_read(struct openpic_softc *, int);
static void		openpic_write(struct openpic_softc *, int, u_int);
static int		openpic_read_irq(struct openpic_softc *, int);
static void		openpic_eoi(struct openpic_softc *, int);
static void		openpic_enable_irq(struct openpic_softc *, int, int);
static void		openpic_disable_irq(struct openpic_softc *, int);
static void		openpic_set_priority(struct openpic_softc *, int, int);
static void		openpic_intr(void);
static void		irq_enable(int);
static void		irq_disable(int);

/*
 * Driver methods.
 */
static device_method_t	openpic_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		openpic_probe),
	DEVMETHOD(device_attach,	openpic_attach),

	/* PIC interface */
	DEVMETHOD(pic_allocate_intr,	openpic_allocate_intr),
	DEVMETHOD(pic_setup_intr,	openpic_setup_intr),
	DEVMETHOD(pic_teardown_intr,	openpic_teardown_intr),
	DEVMETHOD(pic_release_intr,	openpic_release_intr),

	{ 0, 0 }
};

static driver_t	openpic_driver = {
	"openpic",
	openpic_methods,
	sizeof(struct openpic_softc)
};

static devclass_t	openpic_devclass;

DRIVER_MODULE(openpic, nexus, openpic_driver, openpic_devclass, 0, 0);

static struct	openpic_softc *softc;	/* XXX This limits us to one openpic */

/*
 * Device interface
 */

static int
openpic_probe(device_t dev)
{
	struct		openpic_softc *sc;
	phandle_t	node, parent;
	char		*type;
	char            *compat;
	u_int32_t	reg[5], val;
	vm_offset_t	macio_base;
	vm_offset_t     opic_base;
	
	sc = device_get_softc(dev);
	node = nexus_get_node(dev);
	type = nexus_get_device_type(dev);
	compat = nexus_get_compatible(dev);

	if (type == NULL)
		return (ENXIO);

	if (strcmp(type, "open-pic") != 0)
		return (ENXIO);

	if (strcmp(compat, "psim,open-pic") == 0) {
		sc->sc_psim = 1;

		if (OF_getprop(node, "reg", reg, sizeof(reg)) < 8)
			return (ENXIO);

		opic_base = reg[1];
	} else {
		parent = OF_parent(node);
		if (OF_getprop(parent, "assigned-addresses", 
			       reg, sizeof(reg)) < 20)
			return (ENXIO);
		
		macio_base = (vm_offset_t)reg[2];
		
		if (OF_getprop(node, "reg", reg, sizeof(reg)) < 8)
			return (ENXIO);

		opic_base = macio_base + reg[0];
	}

	sc->sc_base = (vm_offset_t)pmap_mapdev(opic_base, OPENPIC_SIZE);

	val = openpic_read(sc, OPENPIC_FEATURE);
	switch (val & OPENPIC_FEATURE_VERSION_MASK) {
	case 1:
		sc->sc_version = "1.0";
		break;
	case 2:
		sc->sc_version = "1.2";
		break;
	case 3:
		sc->sc_version = "1.3";
		break;
	default:
		sc->sc_version = "unknown";
		break;
	}

	sc->sc_ncpu = ((val & OPENPIC_FEATURE_LAST_CPU_MASK) >>
	    OPENPIC_FEATURE_LAST_CPU_SHIFT) + 1;
	sc->sc_nirq = ((val & OPENPIC_FEATURE_LAST_IRQ_MASK) >>
	    OPENPIC_FEATURE_LAST_IRQ_SHIFT) + 1;

	device_set_desc(dev, "OpenPIC interrupt controller");
	return (0);
}

static int
openpic_attach(device_t dev)
{
	struct		openpic_softc *sc;
	u_int32_t	irq, x;

	sc = device_get_softc(dev);
	softc = sc;

	device_printf(dev,
	    "Version %s, supports %d CPUs and %d irqs\n",
	    sc->sc_version, sc->sc_ncpu, sc->sc_nirq);

	sc->sc_rman.rm_type = RMAN_ARRAY;
	sc->sc_rman.rm_descr = device_get_nameunit(dev);

	if (rman_init(&sc->sc_rman) != 0 ||
	    rman_manage_region(&sc->sc_rman, 0, sc->sc_nirq - 1) != 0) {
		device_printf(dev, "could not set up resource management");
		return (ENXIO);
	}

	/* disable all interrupts */
	for (irq = 0; irq < sc->sc_nirq; irq++)
		openpic_write(sc, OPENPIC_SRC_VECTOR(irq), OPENPIC_IMASK);

	openpic_set_priority(sc, 0, 15);

	/* we don't need 8259 passthrough mode */
	x = openpic_read(sc, OPENPIC_CONFIG);
	x |= OPENPIC_CONFIG_8259_PASSTHRU_DISABLE;
	openpic_write(sc, OPENPIC_CONFIG, x);

	/* send all interrupts to cpu 0 */
	for (irq = 0; irq < sc->sc_nirq; irq++)
		openpic_write(sc, OPENPIC_IDEST(irq), 1 << 0);

	for (irq = 0; irq < sc->sc_nirq; irq++) {
		x = irq;
		x |= OPENPIC_IMASK;
		x |= OPENPIC_POLARITY_POSITIVE;
		x |= OPENPIC_SENSE_LEVEL;
		x |= 8 << OPENPIC_PRIORITY_SHIFT;
		openpic_write(sc, OPENPIC_SRC_VECTOR(irq), x);
	}

	/* XXX IPI */
	/* XXX set spurious intr vector */

	openpic_set_priority(sc, 0, 0);

	/* clear all pending interrupts */
	for (irq = 0; irq < sc->sc_nirq; irq++) {
		openpic_read_irq(sc, 0);
		openpic_eoi(sc, 0);
	}

	for (irq = 0; irq < sc->sc_nirq; irq++)
		openpic_disable_irq(sc, irq);

	intr_init(openpic_intr, sc->sc_nirq, irq_enable, irq_disable);

	return (0);
}

/*
 * PIC interface
 */

static struct resource *
openpic_allocate_intr(device_t dev, device_t child, int *rid, u_long intr,
    u_int flags)
{
	struct	openpic_softc *sc;
	struct	resource *rv;
	int	needactivate;

	sc = device_get_softc(dev);
	needactivate = flags & RF_ACTIVE;
	flags &= ~RF_ACTIVE;

	rv = rman_reserve_resource(&sc->sc_rman, intr, intr, 1, flags, child);
	if (rv == NULL) {
		device_printf(dev, "interrupt reservation failed for %s\n",
		    device_get_nameunit(child));
		return (NULL);
	}

	if (needactivate) {
		if (bus_activate_resource(child, SYS_RES_IRQ, *rid, rv) != 0) {
			device_printf(dev,
			    "resource activation failed for %s\n",
			    device_get_nameunit(child));
			rman_release_resource(rv);
			return (NULL);
		}
	}

	return (rv);
}

static int
openpic_setup_intr(device_t dev, device_t child, struct resource *res,
    int flags, driver_intr_t *intr, void *arg, void **cookiep)
{
	struct	openpic_softc *sc;
	int	error;

	sc = device_get_softc(dev);

	if (res == NULL) {
		device_printf(dev, "null interrupt resource from %s\n",
		    device_get_nameunit(child));
		return (EINVAL);
	}

	if ((res->r_flags & RF_SHAREABLE) == 0)
		flags |= INTR_EXCL;

	/*
	 * We depend here on rman_activate_resource() being idempotent.
	 */
	error = rman_activate_resource(res);
	if (error)
		return (error);

	error = inthand_add(device_get_nameunit(child), res->r_start, intr,
	    arg, flags, cookiep);
	openpic_enable_irq(sc, res->r_start, IST_LEVEL);

	return (error);
}

static int
openpic_teardown_intr(device_t dev, device_t child, struct resource *res,
    void *ih)
{
	int	error;

	error = rman_deactivate_resource(res);
	if (error)
		return (error);

	error = inthand_remove(res->r_start, ih);

	return (error);
}

static int
openpic_release_intr(device_t dev, device_t child, int rid,
    struct resource *res)
{
	int	error;

	if (rman_get_flags(res) & RF_ACTIVE) {
		error = bus_deactivate_resource(child, SYS_RES_IRQ, rid, res);
		if (error)
			return (error);
	}

	return (rman_release_resource(res));
}

/*
 * Local routines
 */

static u_int
openpic_read(struct openpic_softc *sc, int reg)
{
	volatile unsigned char *addr;

	addr = (unsigned char *)sc->sc_base + reg;
#if 0
	printf("openpic: reading from %p (0x%08x + 0x%08x)\n", addr,
	    sc->sc_base, reg);
#endif

	return in32rb(addr);
}

static void
openpic_write(struct openpic_softc *sc, int reg, u_int val)
{
	volatile unsigned char *addr;

	addr = (unsigned char *)sc->sc_base + reg;
#if 0
	printf("openpic: writing to %p (0x%08x + 0x%08x)\n", addr, sc->sc_base,
	    reg);
#endif

	out32rb(addr, val);
}

static int
openpic_read_irq(struct openpic_softc *sc, int cpu)
{
	return openpic_read(sc, OPENPIC_IACK(cpu)) & OPENPIC_VECTOR_MASK;
}

static void
openpic_eoi(struct openpic_softc *sc, int cpu)
{
	openpic_write(sc, OPENPIC_EOI(cpu), 0);
	if (!sc->sc_psim) {
		/*
		 * Probably not needed, since appropriate eieio/sync
		 * is done in out32rb. See Darwin src.
		 */
		openpic_read(sc, OPENPIC_EOI(cpu));
	}
}

static void
openpic_enable_irq(struct openpic_softc *sc, int irq, int type)
{
	u_int	x;

	x = openpic_read(sc, OPENPIC_SRC_VECTOR(irq));
	x &= ~(OPENPIC_IMASK | OPENPIC_SENSE_LEVEL | OPENPIC_SENSE_EDGE);
	if (type == IST_LEVEL)
		x |= OPENPIC_SENSE_LEVEL;
	else
		x |= OPENPIC_SENSE_EDGE;
	openpic_write(sc, OPENPIC_SRC_VECTOR(irq), x);
}

static void
openpic_disable_irq(struct openpic_softc *sc, int irq)
{
	u_int	x;

	x = openpic_read(sc, OPENPIC_SRC_VECTOR(irq));
	x |= OPENPIC_IMASK;
	openpic_write(sc, OPENPIC_SRC_VECTOR(irq), x);
}

static void
openpic_set_priority(struct openpic_softc *sc, int cpu, int pri)
{
	u_int	x;

	x = openpic_read(sc, OPENPIC_CPU_PRIORITY(cpu));
	x &= ~OPENPIC_CPU_PRIORITY_MASK;
	x |= pri;
	openpic_write(sc, OPENPIC_CPU_PRIORITY(cpu), x);
}

static void
openpic_intr(void)
{
	int		irq;
	u_int32_t	msr;

	msr = mfmsr();

	irq = openpic_read_irq(softc, 0);
	if (irq == 255) {
		return;
	}

start:
	openpic_disable_irq(softc, irq);
	/*mtmsr(msr | PSL_EE);*/

	/* do the interrupt thang */
	intr_handle(irq);

	mtmsr(msr);

	openpic_eoi(softc, 0);

	irq = openpic_read_irq(softc, 0);
	if (irq != 255)
		goto start;
}

static void
irq_enable(int irq)
{

	openpic_enable_irq(softc, irq, IST_LEVEL);
}

static void
irq_disable(int irq)
{

	openpic_disable_irq(softc, irq);
}
