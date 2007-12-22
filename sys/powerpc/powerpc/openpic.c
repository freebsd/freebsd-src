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
#include <sys/rman.h>

#include <machine/bus.h>
#include <machine/intr.h>
#include <machine/intr_machdep.h>
#include <machine/md_var.h>
#include <machine/pio.h>
#include <machine/resource.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/openpicreg.h>
#include <machine/openpicvar.h>

#include "pic_if.h"

devclass_t openpic_devclass;

/*
 * Local routines
 */

static __inline uint32_t
openpic_read(struct openpic_softc *sc, u_int reg)
{
	return (bus_space_read_4(sc->sc_bt, sc->sc_bh, reg));
}

static __inline void
openpic_write(struct openpic_softc *sc, u_int reg, uint32_t val)
{
	bus_space_write_4(sc->sc_bt, sc->sc_bh, reg, val);
}

static __inline void
openpic_set_priority(struct openpic_softc *sc, int cpu, int pri)
{
	uint32_t x;

	x = openpic_read(sc, OPENPIC_CPU_PRIORITY(cpu));
	x &= ~OPENPIC_CPU_PRIORITY_MASK;
	x |= pri;
	openpic_write(sc, OPENPIC_CPU_PRIORITY(cpu), x);
}

int
openpic_attach(device_t dev)
{
	struct openpic_softc *sc;
	u_int     irq;
	u_int32_t x;

	sc = device_get_softc(dev);
	sc->sc_dev = dev;

	sc->sc_rid = 0;
	sc->sc_memr = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &sc->sc_rid,
	    RF_ACTIVE);

	if (sc->sc_memr == NULL) {
		device_printf(dev, "Could not alloc mem resource!\n");
		return (ENXIO);
	}

	sc->sc_bt = rman_get_bustag(sc->sc_memr);
	sc->sc_bh = rman_get_bushandle(sc->sc_memr);

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
		x = irq;		/* irq == vector. */
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
		(void)openpic_read(sc, OPENPIC_IACK(0));
		openpic_write(sc, OPENPIC_EOI(0), 0);
	}

	powerpc_register_pic(dev);

	return (0);
}

/*
 * PIC I/F methods
 */

void
openpic_dispatch(device_t dev, struct trapframe *tf)
{
	struct openpic_softc *sc;
	u_int vector;

	sc = device_get_softc(dev);
	while (1) {
		vector = openpic_read(sc, OPENPIC_IACK(0));
		vector &= OPENPIC_VECTOR_MASK;
		if (vector == 255)
			break;
		powerpc_dispatch_intr(vector, tf);
	}
}

void
openpic_enable(device_t dev, u_int irq, u_int vector)
{
	struct openpic_softc *sc;
	uint32_t x;

	sc = device_get_softc(dev);
	x = openpic_read(sc, OPENPIC_SRC_VECTOR(irq));
	x &= ~(OPENPIC_IMASK | OPENPIC_VECTOR_MASK);
	x |= vector;
	openpic_write(sc, OPENPIC_SRC_VECTOR(irq), x);
}

void
openpic_eoi(device_t dev, u_int irq __unused)
{
	struct openpic_softc *sc;

	sc = device_get_softc(dev);
	openpic_write(sc, OPENPIC_EOI(0), 0);
}

void
openpic_mask(device_t dev, u_int irq)
{
	struct openpic_softc *sc;
	uint32_t x;

	sc = device_get_softc(dev);
	x = openpic_read(sc, OPENPIC_SRC_VECTOR(irq));
	x |= OPENPIC_IMASK;
	openpic_write(sc, OPENPIC_SRC_VECTOR(irq), x);
	openpic_write(sc, OPENPIC_EOI(0), 0);
}

void
openpic_unmask(device_t dev, u_int irq)
{
	struct openpic_softc *sc;
	uint32_t x;

	sc = device_get_softc(dev);
	x = openpic_read(sc, OPENPIC_SRC_VECTOR(irq));
	x &= ~OPENPIC_IMASK;
	openpic_write(sc, OPENPIC_SRC_VECTOR(irq), x);
}
