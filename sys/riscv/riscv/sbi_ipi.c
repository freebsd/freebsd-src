/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021 Jessica Clarke <jrtc27@FreeBSD.org>
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/cpuset.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/rman.h>

#include <machine/intr.h>
#include <machine/sbi.h>
#include <machine/smp.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include "pic_if.h"

struct sbi_ipi_softc {
	device_t		dev;
	struct resource		*irq_res;
	void			*ih;
	struct intr_irqsrc	isrc;
	uint32_t		pending_ipis[MAXCPU];
};

static void
sbi_ipi_pic_ipi_send(device_t dev, struct intr_irqsrc *isrc, cpuset_t cpus,
    u_int ipi)
{
	struct sbi_ipi_softc *sc;
	struct pcpu *pc;
	u_long mask;
	u_int cpu;

	sc = device_get_softc(dev);

	KASSERT(isrc == &sc->isrc, ("%s: not the IPI isrc", __func__));
	KASSERT(ipi < INTR_IPI_COUNT,
	    ("%s: not a valid IPI: %u", __func__, ipi));

	mask = 0;
	STAILQ_FOREACH(pc, &cpuhead, pc_allcpu) {
		cpu = pc->pc_cpuid;
		if (CPU_ISSET(cpu, &cpus)) {
			atomic_set_32(&sc->pending_ipis[cpu], 1u << ipi);
			mask |= (1ul << pc->pc_hart);
		}
	}
	sbi_send_ipi(&mask);
}

static int
sbi_ipi_pic_ipi_setup(device_t dev, u_int ipi, struct intr_irqsrc **isrcp)
{
	struct sbi_ipi_softc *sc;

	sc = device_get_softc(dev);

	KASSERT(ipi < INTR_IPI_COUNT,
	    ("%s: not a valid IPI: %u", __func__, ipi));

	*isrcp = &sc->isrc;

	return (0);

}

static int
sbi_ipi_intr(void *arg)
{
	struct sbi_ipi_softc *sc;
	uint32_t ipi_bitmap;
	u_int cpu, ipi;
	int bit;

	sc = arg;

	csr_clear(sip, SIP_SSIP);

	cpu = PCPU_GET(cpuid);

	mb();

	ipi_bitmap = atomic_readandclear_32(&sc->pending_ipis[cpu]);
	if (ipi_bitmap == 0)
		return (FILTER_HANDLED);

	mb();

	while ((bit = ffs(ipi_bitmap))) {
		ipi = (bit - 1);
		ipi_bitmap &= ~(1u << ipi);

		intr_ipi_dispatch(ipi);
	}

	return (FILTER_HANDLED);
}

static int
sbi_ipi_probe(device_t dev)
{
	device_set_desc(dev, "RISC-V SBI Inter-Processor Interrupts");

	return (BUS_PROBE_NOWILDCARD);
}

static int
sbi_ipi_attach(device_t dev)
{
	struct sbi_ipi_softc *sc;
	const char *name;
	int irq, rid, error;
	phandle_t iparent;
	pcell_t cell;

	sc = device_get_softc(dev);
	sc->dev = dev;

	memset(sc->pending_ipis, 0, sizeof(sc->pending_ipis));

	name = device_get_nameunit(dev);
	error = intr_isrc_register(&sc->isrc, sc->dev, INTR_ISRCF_IPI,
	    "%s,ipi", name);
	if (error != 0) {
		device_printf(dev, "Can't register interrupt: %d\n", error);
		return (ENXIO);
	}

	iparent = OF_xref_from_node(ofw_bus_get_node(intr_irq_root_dev));
	cell = IRQ_SOFTWARE_SUPERVISOR;
	irq = ofw_bus_map_intr(dev, iparent, 1, &cell);
	error = bus_set_resource(dev, SYS_RES_IRQ, 0, irq, 1);
	if (error != 0) {
		device_printf(dev, "Unable to register IRQ resource\n");
		return (ENXIO);
	}

	rid = 0;
	sc->irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
	    RF_ACTIVE | RF_SHAREABLE);
	if (sc->irq_res == NULL) {
		device_printf(dev, "Unable to alloc IRQ resource\n");
		return (ENXIO);
	}

	error = bus_setup_intr(dev, sc->irq_res, INTR_TYPE_CLK,
	    sbi_ipi_intr, NULL, sc, &sc->ih);
	if (error != 0) {
		device_printf(dev, "Unable to setup IRQ resource\n");
		return (ENXIO);
	}

	/* TODO: Define a set of priorities once other IPI sources exist */
	error = intr_ipi_pic_register(dev, 0);
	if (error != 0) {
		device_printf(dev, "Can't register as IPI source: %d\n", error);
		return (ENXIO);
	}

	return (0);
}

static device_method_t sbi_ipi_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		sbi_ipi_probe),
	DEVMETHOD(device_attach,	sbi_ipi_attach),

	/* Interrupt controller interface */
	DEVMETHOD(pic_ipi_send,		sbi_ipi_pic_ipi_send),
	DEVMETHOD(pic_ipi_setup,	sbi_ipi_pic_ipi_setup),

	DEVMETHOD_END
};

DEFINE_CLASS_0(sbi_ipi, sbi_ipi_driver, sbi_ipi_methods,
    sizeof(struct sbi_ipi_softc));
/* Local interrupt controller attaches during BUS_PASS_ORDER_FIRST */
EARLY_DRIVER_MODULE(sbi_ipi, sbi, sbi_ipi_driver, 0, 0,
    BUS_PASS_INTERRUPT + BUS_PASS_ORDER_EARLY);
