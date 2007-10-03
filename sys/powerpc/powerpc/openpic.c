/*-
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

#include <machine/bus.h>
#include <machine/intr.h>
#include <machine/intr_machdep.h>
#include <machine/md_var.h>
#include <machine/pio.h>
#include <machine/resource.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <sys/rman.h>

#include <machine/openpicreg.h>
#include <machine/openpicvar.h>

#include "pic_if.h"

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
static void		openpic_ext_enable_irq(uintptr_t);
static void		openpic_ext_disable_irq(uintptr_t);

/* XXX This limits us to one openpic */
static struct	openpic_softc *openpic_softc;

/*
 * Called at nexus-probe time to allow interrupts to be enabled by
 * devices that are probed before the OpenPIC h/w is probed.
 */
int
openpic_early_attach(device_t dev)
{
	struct		openpic_softc *sc;

	sc = device_get_softc(dev);
	openpic_softc = sc;

	sc->sc_rman.rm_type = RMAN_ARRAY;
	sc->sc_rman.rm_descr = device_get_nameunit(dev);

	if (rman_init(&sc->sc_rman) != 0 ||
	    rman_manage_region(&sc->sc_rman, 0, OPENPIC_IRQMAX-1) != 0) {
		device_printf(dev, "could not set up resource management");
		return (ENXIO);
        }	

	intr_init(openpic_intr, OPENPIC_IRQMAX, openpic_ext_enable_irq, 
	    openpic_ext_disable_irq);

	sc->sc_early_done = 1;

	return (0);
}

int
openpic_attach(device_t dev)
{
	struct openpic_softc *sc;
	u_int     irq;
	u_int32_t x;

	sc = device_get_softc(dev);
	sc->sc_hwprobed = 1;

	if (!sc->sc_early_done)
		openpic_early_attach(dev);

	x = openpic_read(sc, OPENPIC_FEATURE);
	switch (x & OPENPIC_FEATURE_VERSION_MASK) {
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

	sc->sc_ncpu = ((x & OPENPIC_FEATURE_LAST_CPU_MASK) >>
	    OPENPIC_FEATURE_LAST_CPU_SHIFT) + 1;
	sc->sc_nirq = ((x & OPENPIC_FEATURE_LAST_IRQ_MASK) >>
	    OPENPIC_FEATURE_LAST_IRQ_SHIFT) + 1;

	/*
	 * PSIM seems to report 1 too many IRQs
	 */
	if (sc->sc_psim)
		sc->sc_nirq--;

	if (bootverbose)
		device_printf(dev,
		    "Version %s, supports %d CPUs and %d irqs\n",
		    sc->sc_version, sc->sc_ncpu, sc->sc_nirq);

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

	/* enable pre-h/w reserved irqs, disable all others */
	for (irq = 0; irq < sc->sc_nirq; irq++)
		if (sc->sc_irqrsv[irq])
			openpic_enable_irq(sc, irq, IST_LEVEL);
		else
			openpic_disable_irq(sc, irq);

	return (0);
}

/*
 * PIC interface
 */

struct resource *
openpic_allocate_intr(device_t dev, device_t child, int *rid, u_long intr,
    u_int flags)
{
	struct	openpic_softc *sc;
	struct	resource *rv;
	int	needactivate;

	sc = device_get_softc(dev);
	needactivate = flags & RF_ACTIVE;
	flags &= ~RF_ACTIVE;

	if (sc->sc_hwprobed && (intr > sc->sc_nirq)) {
		device_printf(dev, "interrupt reservation %ld out of range\n",
		    intr);
		return (NULL);
	}

	rv = rman_reserve_resource(&sc->sc_rman, intr, intr, 1, flags, child);
	if (rv == NULL) {
		device_printf(dev, "interrupt reservation failed for %s\n",
		    device_get_nameunit(child));
		return (NULL);
	}
	rman_set_rid(rv, *rid);
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

int
openpic_setup_intr(device_t dev, device_t child, struct resource *res,
    int flags, driver_intr_t *intr, void *arg, void **cookiep)
{
	struct	openpic_softc *sc;
	u_long	start;
	int	error;

	sc = device_get_softc(dev);
	start = rman_get_start(res);

	if (res == NULL) {
		device_printf(dev, "null interrupt resource from %s\n",
		    device_get_nameunit(child));
		return (EINVAL);
	}

	if ((rman_get_flags(res) & RF_SHAREABLE) == 0)
		flags |= INTR_EXCL;

	/*
	 * We depend here on rman_activate_resource() being idempotent.
	 */
	error = rman_activate_resource(res);
	if (error)
		return (error);

	error = inthand_add(device_get_nameunit(child), start, intr, arg,
	    flags, cookiep);

	if (sc->sc_hwprobed)
		openpic_enable_irq(sc, start, IST_LEVEL);
	else
		sc->sc_irqrsv[start] = 1;

	return (error);
}

int
openpic_teardown_intr(device_t dev, device_t child, struct resource *res,
    void *ih)
{
	int	error;

	error = rman_deactivate_resource(res);
	if (error)
		return (error);

	error = inthand_remove(rman_get_start(res), ih);

	return (error);
}

int
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
	return (bus_space_read_4(sc->sc_bt, sc->sc_bh, reg));
}

static void
openpic_write(struct openpic_softc *sc, int reg, u_int val)
{
	bus_space_write_4(sc->sc_bt, sc->sc_bh, reg, val);
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
	struct openpic_softc *sc;
	int		irq;
	u_int32_t	msr;

	sc = openpic_softc;
	msr = mfmsr();

	irq = openpic_read_irq(sc, 0);
	if (irq == 255) {
		return;
	}

start:
	openpic_disable_irq(sc, irq);
	/*mtmsr(msr | PSL_EE);*/

	/* do the interrupt thang */
	intr_handle(irq);

	mtmsr(msr);

	openpic_eoi(sc, 0);

	irq = openpic_read_irq(sc, 0);
	if (irq != 255)
		goto start;
}

static void
openpic_ext_enable_irq(uintptr_t irq)
{
	if (!openpic_softc->sc_hwprobed)
		return;

	openpic_enable_irq(openpic_softc, irq, IST_LEVEL);
}

static void
openpic_ext_disable_irq(uintptr_t irq)
{
	if (!openpic_softc->sc_hwprobed)
		return;

	openpic_disable_irq(openpic_softc, irq);
}
