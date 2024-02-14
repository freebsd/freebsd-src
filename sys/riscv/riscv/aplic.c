/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 Himanshu Chauhan <hchauhan@thechauhan.dev>
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
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/module.h>
#include <sys/proc.h>
#include <sys/rman.h>
#include <sys/smp.h>

#include <machine/bus.h>
#include <machine/intr.h>
#include <machine/riscvreg.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include "pic_if.h"

#define	APLIC_MAX_IRQS		1023

/* Smaller priority number means higher priority */
#define	APLIC_INTR_DEF_PRIO	1

static pic_disable_intr_t	aplic_disable_intr;
static pic_enable_intr_t	aplic_enable_intr;
static pic_map_intr_t		aplic_map_intr;
static pic_setup_intr_t		aplic_setup_intr;
static pic_post_ithread_t	aplic_post_ithread;
static pic_pre_ithread_t	aplic_pre_ithread;
static pic_bind_intr_t		aplic_bind_intr;

struct aplic_irqsrc {
	struct intr_irqsrc isrc;
	u_int irq;
};

struct aplic_softc {
	device_t dev;
	struct resource *mem_res;
	struct resource *irq_res;
	void *ih;
	struct aplic_irqsrc isrcs[APLIC_MAX_IRQS + 1];
	unsigned int hart_indices[MAXCPU];
	int ndev;
};

#define	APLIC_DOMAIN_CFG_IE		(1UL << 8)	/* Enable domain IRQs */
#define	APLIC_DOMAIN_CFG_DM		(1UL << 2)	/* IRQ delivery mode */
#define	APLIC_DOMAIN_CFG_BE		(1UL << 0)	/* Endianess */

#define	APLIC_MODE_DIRECT		0	/* Direct delivery mode */
#define	APLIC_MODE_MSI			1	/* MSI delivery mode */

#define	APLIC_SRC_CFG_DLGT		(1UL << 10)	/* Source delegation */
#define	APLIC_SRC_CFG_SM_SHIFT		0
#define	APLIC_SRC_CFG_SM_MASK		(0x7UL << APLIC_SRC_CFG_SM_SHIFT)

#define	APLIC_SRC_CFG_SM_INACTIVE	0	/* APLIC inactive in domain */
#define	APLIC_SRC_CFG_SM_DETACHED	1	/* Detached from source wire */
#define	APLIC_SRC_CFG_SM_EDGE_RSE	4	/* Asserted on rising edge */
#define	APLIC_SRC_CFG_SM_EDGE_FLL	5	/* Asserted on falling edge */
#define	APLIC_SRC_CFG_SM_LVL_HI		6	/* Asserted when high */
#define	APLIC_SRC_CFG_SM_LVL_LO		7	/* Asserted when low */

/* Register offsets in APLIC configuration space */
#define	APLIC_DOMAIN_CFG		0x0000
#define	APLIC_SRC_CFG(_idx)		(0x0004 + (((_idx) - 1) * 4))
#define	APLIC_TARGET(_idx)		(0x3004 + (((_idx) - 1) * 4))
#define	APLIC_MMSIADDRCFG		0x1BC0
#define	APLIC_MMSIADDRCFGH		0x1BC4
#define	APLIC_SMSIADDRCFG		0x1BC8
#define	APLIC_SMSIADDRCFGH		0x1BCC
#define	APLIC_SETIPNUM			0x1CDC
#define	APLIC_CLRIPNUM			0x1DDC
#define	APLIC_SETIENUM			0x1EDC
#define	APLIC_CLRIENUM			0x1FDC
#define	APLIC_SETIPNUM_LE		0x2000
#define	APLIC_SETIPNUM_BE		0x2004
#define	APLIC_GENMSI			0x3000
#define	APLIC_IDC_BASE			0x4000

#define	APLIC_SETIE_BASE		0x1E00
#define	APLIC_CLRIE_BASE		0x1F00

/* Interrupt delivery control structure */
#define	APLIC_IDC_IDELIVERY_OFFS	0x0000
#define	APLIC_IDC_IFORCE_OFFS		0x0004
#define	APLIC_IDC_ITHRESHOLD_OFFS	0x0008
#define	APLIC_IDC_TOPI_OFFS		0x0018
#define	APLIC_IDC_CLAIMI_OFFS		0x001C

#define	APLIC_IDC_SZ			0x20

#define	APLIC_IDC_IDELIVERY_DISABLE	0
#define	APLIC_IDC_IDELIVERY_ENABLE	1
#define	APLIC_IDC_ITHRESHOLD_DISABLE	0

#define	APLIC_IDC_CLAIMI_PRIO_MASK	0xff
#define	APLIC_IDC_CLAIMI_IRQ_SHIFT	16
#define	APLIC_IDC_CLAIMI_IRQ_MASK	0x3ff

#define	APLIC_IDC_CLAIMI_IRQ(_claimi)			\
	(((_claimi) >> APLIC_IDC_CLAIMI_IRQ_SHIFT)	\
	    & APLIC_IDC_CLAIMI_IRQ_MASK)

#define	APLIC_IDC_CLAIMI_PRIO(_claimi)	((_claimi) & APLIC_IDC_CLAIMI_PRIO_MASK)

#define	APLIC_IDC_REG(_sc, _cpu, _field)		\
	(APLIC_IDC_BASE + APLIC_IDC_##_field##_OFFS +	\
	    ((_sc->hart_indices[_cpu]) * APLIC_IDC_SZ))

#define	APLIC_IDC_IDELIVERY(_sc, _cpu)			\
	APLIC_IDC_REG(_sc, _cpu, IDELIVERY)

#define	APLIC_IDC_IFORCE(_sc, _cpu)			\
	APLIC_IDC_REG(_sc, _cpu, IFORCE)

#define	APLIC_IDC_ITHRESHOLD(_sc, _cpu)			\
	APLIC_IDC_REG(_sc, _cpu, ITHRESHOLD)

#define	APLIC_IDC_TOPI(_sc, _cpu)			\
	APLIC_IDC_REG(_sc, _cpu, TOPI)

#define	APLIC_IDC_CLAIMI(_sc, _cpu)			\
	APLIC_IDC_REG(_sc, _cpu, CLAIMI)

#define	APLIC_MK_IRQ_TARGET(_sc, _cpu, _prio)		\
	(_sc->hart_indices[_cpu] << 18 | ((_prio) & 0xff))

#define	aplic_read(sc, reg)		bus_read_4(sc->mem_res, (reg))
#define	aplic_write(sc, reg, val)	bus_write_4(sc->mem_res, (reg), (val))

static u_int aplic_irq_cpu;

static inline int
riscv_cpu_to_hartid(int cpu)
{
	return pcpu_find(cpu)->pc_hart;
}

static inline int
riscv_hartid_to_cpu(int hartid)
{
	int cpu;

	CPU_FOREACH(cpu) {
		if (riscv_cpu_to_hartid(cpu) == hartid)
			return cpu;
	}

	return (-1);
}

static int
fdt_get_hartid(device_t dev, phandle_t aplic)
{
	int hartid;

	/* Check the interrupt controller layout. */
	if (OF_searchencprop(aplic, "#interrupt-cells", &hartid,
	    sizeof(hartid)) == -1) {
		device_printf(dev,
		    "Could not find #interrupt-cells for phandle %u\n", aplic);
		return (-1);
	}

	/*
	 * The parent of the interrupt-controller is the CPU we are
	 * interested in, so search for its hart ID.
	 */
	if (OF_searchencprop(OF_parent(aplic), "reg", (pcell_t *)&hartid,
	    sizeof(hartid)) == -1) {
		device_printf(dev, "Could not find hartid\n");
		return (-1);
	}

	return (hartid);
}

static inline void
aplic_irq_dispatch(struct aplic_softc *sc, u_int irq, u_int prio,
    struct trapframe *tf)
{
	struct aplic_irqsrc *src;

	src = &sc->isrcs[irq];

	if (intr_isrc_dispatch(&src->isrc, tf) != 0)
		if (bootverbose)
			device_printf(sc->dev, "Stray irq %u detected\n", irq);
}

static int
aplic_intr(void *arg)
{
	struct aplic_softc *sc;
	struct trapframe *tf;
	u_int claimi, prio, irq;
	int cpu;

	sc = arg;
	cpu = PCPU_GET(cpuid);

	/* Claim any pending interrupt. */
	claimi = aplic_read(sc, APLIC_IDC_CLAIMI(sc, cpu));
	prio = APLIC_IDC_CLAIMI_PRIO(claimi);
	irq = APLIC_IDC_CLAIMI_IRQ(claimi);

	KASSERT((irq != 0), ("Invalid IRQ 0"));

	tf = curthread->td_intr_frame;
	aplic_irq_dispatch(sc, irq, prio, tf);

	return (FILTER_HANDLED);
}

static void
aplic_disable_intr(device_t dev, struct intr_irqsrc *isrc)
{
	struct aplic_softc *sc;
	struct aplic_irqsrc *src;

	sc = device_get_softc(dev);
	src = (struct aplic_irqsrc *)isrc;

	/* Disable the interrupt source */
	aplic_write(sc, APLIC_CLRIENUM, src->irq);
}

static void
aplic_enable_intr(device_t dev, struct intr_irqsrc *isrc)
{
	struct aplic_softc *sc;
	struct aplic_irqsrc *src;

	sc = device_get_softc(dev);
	src = (struct aplic_irqsrc *)isrc;

	/* Enable the interrupt source */
	aplic_write(sc, APLIC_SETIENUM, src->irq);
}

static int
aplic_map_intr(device_t dev, struct intr_map_data *data,
    struct intr_irqsrc **isrcp)
{
	struct intr_map_data_fdt *daf;
	struct aplic_softc *sc;

	sc = device_get_softc(dev);

	if (data->type != INTR_MAP_DATA_FDT)
		return (ENOTSUP);

	daf = (struct intr_map_data_fdt *)data;
	if (daf->ncells != 2 || daf->cells[0] > sc->ndev) {
		device_printf(dev, "Invalid cell data\n");
		return (EINVAL);
	}

	*isrcp = &sc->isrcs[daf->cells[0]].isrc;

	return (0);
}

static int
aplic_probe(device_t dev)
{
	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (!ofw_bus_is_compatible(dev, "riscv,aplic"))
		return (ENXIO);

	device_set_desc(dev, "Advanced Platform-Level Interrupt Controller");

	return (BUS_PROBE_DEFAULT);
}

/*
 * Setup APLIC in direct mode.
 */
static int
aplic_setup_direct_mode(device_t dev)
{
	struct aplic_irqsrc *isrcs;
	struct aplic_softc *sc;
	struct intr_pic *pic;
	const char *name;
	phandle_t node, xref, iparent;
	pcell_t *cells, cell;
	int error = ENXIO;
	u_int irq;
	int cpu, hartid, rid, i, nintr, idc;

	sc = device_get_softc(dev);
	node = ofw_bus_get_node(dev);

	sc->dev = dev;

	if ((OF_getencprop(node, "riscv,num-sources", &sc->ndev,
	    sizeof(sc->ndev))) < 0) {
		device_printf(dev, "Error: could not get number of devices\n");
		return (error);
	}

	if (sc->ndev > APLIC_MAX_IRQS) {
		device_printf(dev, "Error: invalid ndev (%d)\n", sc->ndev);
		return (error);
	}

	/* Request memory resources */
	rid = 0;
	sc->mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	if (sc->mem_res == NULL) {
		device_printf(dev,
		    "Error: could not allocate memory resources\n");
		return (error);
	}

	/* Set APLIC in direct mode and enable all interrupts */
	aplic_write(sc, APLIC_DOMAIN_CFG,
	    APLIC_MODE_DIRECT | APLIC_DOMAIN_CFG_IE);

	/* Register the interrupt sources */
	isrcs = sc->isrcs;
	name = device_get_nameunit(sc->dev);
	for (irq = 1; irq <= sc->ndev; irq++) {
		isrcs[irq].irq = irq;

		error = intr_isrc_register(&isrcs[irq].isrc, sc->dev,
		    0, "%s,%u", name, irq);
		if (error != 0)
			goto fail;

		aplic_write(sc, APLIC_SRC_CFG(irq),
		    APLIC_SRC_CFG_SM_DETACHED);
	}

	nintr = OF_getencprop_alloc_multi(node, "interrupts-extended",
	    sizeof(uint32_t), (void **)&cells);
	if (nintr <= 0) {
		device_printf(dev, "Could not read interrupts-extended\n");
		goto fail;
	}

	/* interrupts-extended is a list of phandles and interrupt types. */
	for (i = 0, idc = 0; i < nintr; i += 2, idc++) {
		/* Skip M-mode external interrupts */
		if (cells[i + 1] != IRQ_EXTERNAL_SUPERVISOR)
			continue;

		/* Get the hart ID from the CLIC's phandle. */
		hartid = fdt_get_hartid(dev, OF_node_from_xref(cells[i]));
		if (hartid < 0) {
			OF_prop_free(cells);
			goto fail;
		}

		/* Get the corresponding cpuid. */
		cpu = riscv_hartid_to_cpu(hartid);
		if (cpu < 0) {
			device_printf(dev, "Invalid cpu for hart %d\n", hartid);
			OF_prop_free(cells);
			goto fail;
		}

		sc->hart_indices[cpu] = idc;
	}
	OF_prop_free(cells);

	/* Turn off the interrupt delivery for all CPUs within or out domain */
	CPU_FOREACH(cpu) {
		aplic_write(sc, APLIC_IDC_IDELIVERY(sc, cpu),
		    APLIC_IDC_IDELIVERY_DISABLE);
		aplic_write(sc, APLIC_IDC_ITHRESHOLD(sc, cpu),
		    APLIC_IDC_ITHRESHOLD_DISABLE);
	}

	iparent = OF_xref_from_node(ofw_bus_get_node(intr_irq_root_dev));
	cell = IRQ_EXTERNAL_SUPERVISOR;
	irq = ofw_bus_map_intr(dev, iparent, 1, &cell);
	error = bus_set_resource(dev, SYS_RES_IRQ, 0, irq, 1);
	if (error != 0) {
		device_printf(dev, "Unable to register IRQ resource\n");
		return (ENXIO);
	}

	rid = 0;
	sc->irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
	    RF_ACTIVE);
	if (sc->irq_res == NULL) {
		device_printf(dev,
		    "Error: could not allocate IRQ resources\n");
		return (ENXIO);
	}

	xref = OF_xref_from_node(node);
	pic = intr_pic_register(sc->dev, xref);
	if (pic == NULL) {
		error = ENXIO;
		goto fail;
	}

	error = bus_setup_intr(dev, sc->irq_res, INTR_TYPE_CLK,
	    aplic_intr, NULL, sc, &sc->ih);
	if (error != 0) {
		device_printf(dev, "Unable to setup IRQ resource\n");
		return (ENXIO);
	}

fail:
	return (error);
}

static int
aplic_attach(device_t dev)
{
	int rc;

	/* APLIC with IMSIC on hart is not supported */
	if (ofw_bus_has_prop(dev, "msi-parent")) {
		device_printf(dev, "APLIC with IMSIC is unsupported\n");
		return (ENXIO);
	}

	rc = aplic_setup_direct_mode(dev);

	return (rc);
}

static void
aplic_pre_ithread(device_t dev, struct intr_irqsrc *isrc)
{
	aplic_disable_intr(dev, isrc);
}

static void
aplic_post_ithread(device_t dev, struct intr_irqsrc *isrc)
{
	aplic_enable_intr(dev, isrc);
}

static int
aplic_setup_intr(device_t dev, struct intr_irqsrc *isrc, struct resource *res,
    struct intr_map_data *data)
{
	struct aplic_irqsrc *src;
	struct aplic_softc *sc;

	CPU_ZERO(&isrc->isrc_cpu);

	sc = device_get_softc(dev);
	src = (struct aplic_irqsrc *)isrc;

	aplic_write(sc, APLIC_SRC_CFG(src->irq), APLIC_SRC_CFG_SM_EDGE_RSE);

	/*
	 * In uniprocessor system, bind_intr will not be called.
	 * So bind the interrupt on this CPU. If secondary CPUs
	 * are present, then bind_intr will be called again and
	 * interrupts will rebind to those CPUs.
	 */
	aplic_bind_intr(dev, isrc);

	return (0);
}

static int
aplic_bind_intr(device_t dev, struct intr_irqsrc *isrc)
{
	struct aplic_softc *sc;
	struct aplic_irqsrc *src;
	uint32_t cpu, hartid;

	sc = device_get_softc(dev);
	src = (struct aplic_irqsrc *)isrc;

	/* Disable the interrupt source */
	aplic_write(sc, APLIC_CLRIENUM, src->irq);

	if (CPU_EMPTY(&isrc->isrc_cpu)) {
		cpu = aplic_irq_cpu = intr_irq_next_cpu(aplic_irq_cpu,
		    &all_cpus);
		CPU_SETOF(cpu, &isrc->isrc_cpu);
	} else {
		cpu = CPU_FFS(&isrc->isrc_cpu) - 1;
	}

	hartid = riscv_cpu_to_hartid(cpu);

	if (bootverbose)
		device_printf(dev, "Bind irq %d to cpu%d (hart %d)\n", src->irq,
		    cpu, hartid);

	aplic_write(sc, APLIC_TARGET(src->irq),
	    APLIC_MK_IRQ_TARGET(sc, cpu, APLIC_INTR_DEF_PRIO));
	aplic_write(sc, APLIC_IDC_IDELIVERY(sc, cpu),
	    APLIC_IDC_IDELIVERY_ENABLE);
	aplic_enable_intr(dev, isrc);

	return (0);
}

static device_method_t aplic_methods[] = {
	DEVMETHOD(device_probe,		aplic_probe),
	DEVMETHOD(device_attach,	aplic_attach),

	DEVMETHOD(pic_disable_intr,	aplic_disable_intr),
	DEVMETHOD(pic_enable_intr,	aplic_enable_intr),
	DEVMETHOD(pic_map_intr,		aplic_map_intr),
	DEVMETHOD(pic_pre_ithread,	aplic_pre_ithread),
	DEVMETHOD(pic_post_ithread,	aplic_post_ithread),
	DEVMETHOD(pic_post_filter,	aplic_post_ithread),
	DEVMETHOD(pic_setup_intr,	aplic_setup_intr),
	DEVMETHOD(pic_bind_intr,	aplic_bind_intr),

	DEVMETHOD_END
};

DEFINE_CLASS_0(aplic, aplic_driver, aplic_methods, sizeof(struct aplic_softc));

EARLY_DRIVER_MODULE(aplic, simplebus, aplic_driver, 0, 0,
    BUS_PASS_INTERRUPT + BUS_PASS_ORDER_MIDDLE);
