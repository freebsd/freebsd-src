/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Ruslan Bukin <br@bsdpad.com>
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
#include <sys/intr.h>

#include <machine/bus.h>
#include <machine/intr.h>
#include <machine/riscvreg.h>
#include <machine/imsic.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dt-bindings/interrupt-controller/irq.h>

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
	u_int msi;
	u_int type;
};

struct aplic_softc {
	device_t dev;
	struct resource *mem_res;
	struct resource *irq_res;
	void *ih;
	struct aplic_irqsrc isrcs[APLIC_MAX_IRQS + 1];
	u_int hart_indices[MAXCPU];
	device_t parent;
	struct intr_pic *pic;
	int ndev;
	bool has_imsic;
};

/* Register offsets in APLIC configuration space */
#define	APLIC_DOMAIN_CFG	0x0000
#define	 DOMAIN_CFG_IE		(1UL << 8)	/* Enable domain IRQs */
#define	 DOMAIN_CFG_DM		(1UL << 2)	/* IRQ delivery mode */
#define	 DOMAIN_CFG_BE		(1UL << 0)	/* Endianess */
#define	APLIC_SRC_CFG(_idx)	(0x0004 + (((_idx) - 1) * 4))
#define	 SRC_CFG_DLGT		(1UL << 10)	/* Source delegation */
#define	 SRC_CFG_SM_SHIFT	0
#define	 SRC_CFG_SM_MASK	(0x7UL << SRC_CFG_SM_SHIFT)
#define	 SRC_CFG_SM_INACTIVE	0	/* APLIC inactive in domain */
#define	 SRC_CFG_SM_DETACHED	1	/* Detached from source wire */
#define	 SRC_CFG_SM_EDGE_RSE	4	/* Asserted on rising edge */
#define	 SRC_CFG_SM_EDGE_FLL	5	/* Asserted on falling edge */
#define	 SRC_CFG_SM_LVL_HI	6	/* Asserted when high */
#define	 SRC_CFG_SM_LVL_LO	7	/* Asserted when low */
#define	APLIC_TARGET(_idx)	(0x3004 + (((_idx) - 1) * 4))
#define	 TARGET_HART_ID_S	18
#define	 TARGET_HART_ID_M	(0x3fff << TARGET_HART_ID_S)
/* Direct delivery mode. */
#define	 TARGET_IPRIO_S		0
#define	 TARGET_IPRIO_M		(0xff << TARGET_IPRIO_S)
/* MSI delivery mode. */
#define	 TARGET_GUEST_ID_S	12
#define	 TARGET_GUEST_ID_M	(0x3f << TARGET_GUEST_ID_S)
#define	 TARGET_EIID_S		0
#define	 TARGET_EIID_M		(0x7ff << TARGET_EIID_S)
#define	APLIC_MMSIADDRCFG	0x1BC0
#define	APLIC_MMSIADDRCFGH	0x1BC4
#define	APLIC_SMSIADDRCFG	0x1BC8
#define	APLIC_SMSIADDRCFGH	0x1BCC
#define	APLIC_SETIPNUM		0x1CDC
#define	APLIC_CLRIPNUM		0x1DDC
#define	APLIC_SETIE_BASE	0x1E00
#define	APLIC_SETIENUM		0x1EDC
#define	APLIC_CLRIE_BASE	0x1F00
#define	APLIC_CLRIENUM		0x1FDC
#define	APLIC_SETIPNUM_LE	0x2000
#define	APLIC_SETIPNUM_BE	0x2004
#define	APLIC_GENMSI		0x3000
#define	APLIC_IDC_BASE		0x4000

/* Interrupt delivery control structure */
#define	APLIC_IDC_SIZE			0x20
#define	APLIC_IDC_IDELIVERY_OFFS	0x0000
#define	APLIC_IDC_IFORCE_OFFS		0x0004
#define	APLIC_IDC_ITHRESHOLD_OFFS	0x0008
#define	APLIC_IDC_TOPI_OFFS		0x0018
#define	APLIC_IDC_CLAIMI_OFFS		0x001C
#define	APLIC_IDC_REG(_sc, _cpu, _field)		\
	APLIC_IDC_BASE + APLIC_IDC_##_field##_OFFS +	\
	    ((_sc->hart_indices[_cpu]) * APLIC_IDC_SIZE)

#define	APLIC_IDC_IDELIVERY(_sc, _cpu)	APLIC_IDC_REG(_sc, _cpu, IDELIVERY)
#define	 IDELIVERY_DISABLE		0
#define	 IDELIVERY_ENABLE		1
#define	APLIC_IDC_IFORCE(_sc, _cpu)	APLIC_IDC_REG(_sc, _cpu, IFORCE)
#define	APLIC_IDC_ITHRESHOLD(_sc, _cpu) APLIC_IDC_REG(_sc, _cpu, ITHRESHOLD)
#define	 ITHRESHOLD_DISABLE		0
#define	APLIC_IDC_TOPI(_sc, _cpu)	APLIC_IDC_REG(_sc, _cpu, TOPI)
#define	APLIC_IDC_CLAIMI(_sc, _cpu)	APLIC_IDC_REG(_sc, _cpu, CLAIMI)
#define	 CLAIMI_PRIO_MASK		0xff
#define	 CLAIMI_IRQ_SHIFT		16
#define	 CLAIMI_IRQ_MASK		0x3ff
#define	 CLAIMI_PRIO(_r)	((_r) & CLAIMI_PRIO_MASK)
#define	 CLAIMI_IRQ(_r)		(((_r) >> CLAIMI_IRQ_SHIFT) & CLAIMI_IRQ_MASK)

#define	aplic_read(sc, reg)		bus_read_4(sc->mem_res, (reg))
#define	aplic_write(sc, reg, val)	bus_write_4(sc->mem_res, (reg), (val))

static u_int aplic_irq_cpu;

static inline uint32_t
riscv_cpu_to_hartid(int cpu)
{
	return (pcpu_find(cpu)->pc_hart);
}

static inline int
riscv_hartid_to_cpu(uint32_t hartid)
{
	int cpu;

	CPU_FOREACH(cpu) {
		if (riscv_cpu_to_hartid(cpu) == hartid)
			return (cpu);
	}

	return (-1);
}

static int
fdt_get_parent_imsic(device_t dev)
{
	struct aplic_softc *sc;
	phandle_t parent_xref, node;
	int rc;

	sc = device_get_softc(dev);
	node = ofw_bus_get_node(dev);

	rc = OF_getencprop(node, "msi-parent", &parent_xref,
	    sizeof(parent_xref));
	if (rc <= 0) {
		device_printf(dev, "can't read parent node property\n");
		return (ENXIO);
	}

	sc->parent = OF_device_from_xref(parent_xref);
	if (sc->parent == NULL) {
		device_printf(dev, "can't find parent MSI controller\n");
		return (ENXIO);
	}

	return (0);
}

static int
fdt_get_hartid(device_t dev, phandle_t aplic, uint32_t *hartid)
{

	/* Check the interrupt controller layout. */
	if (OF_searchencprop(aplic, "#interrupt-cells", hartid,
	    sizeof(*hartid)) == -1) {
		device_printf(dev,
		    "Could not find #interrupt-cells for phandle %u\n", aplic);
		return (-1);
	}

	/*
	 * The parent of the interrupt-controller is the CPU we are
	 * interested in, so search for its hart ID.
	 */
	if (OF_searchencprop(OF_parent(aplic), "reg", (pcell_t *)hartid,
	    sizeof(*hartid)) == -1) {
		device_printf(dev, "Could not find hartid\n");
		return (-1);
	}

	return (0);
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
aplic_imsic_intr(void *arg, unsigned long irq)
{
	struct aplic_softc *sc;
	struct trapframe *tf;

	sc = arg;
	tf = curthread->td_intr_frame;
	aplic_irq_dispatch(sc, irq, 0, tf);

	return (FILTER_HANDLED);
}

static int
aplic_intr(void *arg)
{
	struct aplic_softc *sc;
	struct trapframe *tf;
	uint32_t claimi;
	u_int prio, irq;
	int cpu;

	sc = arg;
	cpu = PCPU_GET(cpuid);

	/* Claim all pending interrupts. */
	while ((claimi = aplic_read(sc, APLIC_IDC_CLAIMI(sc, cpu))) != 0) {
		prio = CLAIMI_PRIO(claimi);
		irq = CLAIMI_IRQ(claimi);

		KASSERT((irq != 0), ("Invalid IRQ 0"));

		tf = curthread->td_intr_frame;
		aplic_irq_dispatch(sc, irq, prio, tf);
	}

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
	u_int type;
	u_int irq;

	sc = device_get_softc(dev);

	if (data->type != INTR_MAP_DATA_FDT)
		return (ENOTSUP);

	daf = (struct intr_map_data_fdt *)data;
	if (daf->ncells != 2 || daf->cells[0] > sc->ndev) {
		device_printf(dev, "Invalid cell data\n");
		return (EINVAL);
	}

	irq = daf->cells[0];
	type = daf->cells[1];

	sc->isrcs[irq].type = type;
	*isrcp = &sc->isrcs[irq].isrc;

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
	struct aplic_softc *sc;
	phandle_t node;
	pcell_t *cells;
	uint32_t hartid;
	int nintr, idc;
	int cpu, i;
	int error;

	error = ENXIO;

	sc = device_get_softc(dev);
	node = ofw_bus_get_node(dev);

	device_printf(dev, "Configuring APLIC in Direct mode.\n");
	/* Set APLIC in direct mode and enable all interrupts */
	aplic_write(sc, APLIC_DOMAIN_CFG, DOMAIN_CFG_IE);

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
		error = fdt_get_hartid(dev, OF_node_from_xref(cells[i]),
		    &hartid);
		if (error != 0) {
			OF_prop_free(cells);
			goto fail;
		}

		/* Get the corresponding cpuid. */
		cpu = riscv_hartid_to_cpu(hartid);
		if (cpu == -1) {
			device_printf(dev, "Invalid cpu for hart %d\n", hartid);
			OF_prop_free(cells);
			goto fail;
		}

		sc->hart_indices[cpu] = idc;
	}

	/* Turn off the interrupt delivery for all CPUs within or out domain */
	CPU_FOREACH(cpu) {
		aplic_write(sc, APLIC_IDC_IDELIVERY(sc, cpu),
		    IDELIVERY_DISABLE);
		aplic_write(sc, APLIC_IDC_ITHRESHOLD(sc, cpu),
		    ITHRESHOLD_DISABLE);
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
aplic_setup_imsic_mode(device_t dev)
{
	struct aplic_softc *sc;
	int error = ENXIO;

	sc = device_get_softc(dev);

	device_printf(dev, "Configuring APLIC with IMSIC.\n");

	if ((error = fdt_get_parent_imsic(dev)) != 0)
		return (error);

	error = intr_pic_add_handler(sc->parent, sc->pic,
	    aplic_imsic_intr, sc, 1, APLIC_MAX_IRQS);
	if (error != 0) {
		device_printf(dev, "Failed to install child handler, "
		    "error %d\n", error);
		return (error);
	}

	aplic_write(sc, APLIC_DOMAIN_CFG, DOMAIN_CFG_DM | DOMAIN_CFG_IE);

	return (0);
}

static int
aplic_setup(device_t dev)
{
	struct aplic_irqsrc *isrcs;
	struct aplic_softc *sc;
	const char *name;
	phandle_t node, xref, iparent;
	pcell_t cell;
	int error = ENXIO;
	u_int irq;
	int rid;
	device_t rootdev;

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

	/* Register the interrupt sources */
	isrcs = sc->isrcs;
	name = device_get_nameunit(sc->dev);
	for (irq = 1; irq <= sc->ndev; irq++) {
		isrcs[irq].irq = irq;

		error = intr_isrc_register(&isrcs[irq].isrc, sc->dev,
		    0, "%s,%u", name, irq);
		if (error != 0)
			goto fail;

		aplic_write(sc, APLIC_SRC_CFG(irq), SRC_CFG_SM_DETACHED);
	}

	rootdev = intr_irq_root_device(INTR_ROOT_IRQ);
	iparent = OF_xref_from_node(ofw_bus_get_node(rootdev));
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

	if (sc->has_imsic)
		error = aplic_setup_imsic_mode(dev);
	else
		error = aplic_setup_direct_mode(dev);
	if (error != 0)
		goto fail;

	xref = OF_xref_from_node(node);
	sc->pic = intr_pic_register(sc->dev, xref);
	if (sc->pic == NULL) {
		error = ENXIO;
		goto fail;
	}
fail:
	return (error);
}

static int
aplic_attach(device_t dev)
{
	struct aplic_softc *sc;
	int rc;

	sc = device_get_softc(dev);

	if (ofw_bus_has_prop(dev, "msi-parent"))
		sc->has_imsic = true;

	rc = aplic_setup(dev);

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
	struct aplic_softc *sc;
	struct aplic_irqsrc *src;

	sc = device_get_softc(dev);
	src = (struct aplic_irqsrc *)isrc;

	aplic_enable_intr(dev, isrc);

	if (!src->msi)
		return;

	/*
	 * The section "4.9.2 Special consideration for level-sensitive
	 * interrupt sources" of the RISC-V AIA specification says:
	 *
	 * A second option is for the interrupt service routine to write the
	 * APLIC’s source identity number for the interrupt to the domain’s
	 * setipnum register just before exiting. This will cause the
	 * interrupt’s pending bit to be set to one again if the source is
	 * still asserting an interrupt, but not if the source is not asserting
	 * an interrupt.
	 */

	switch (src->type) {
	case IRQ_TYPE_LEVEL_HIGH:
	case IRQ_TYPE_LEVEL_LOW:
		aplic_write(sc, APLIC_SETIPNUM_LE, src->irq);
		break;
	default:
		break;
	}
}

static int
aplic_setup_intr(device_t dev, struct intr_irqsrc *isrc, struct resource *res,
    struct intr_map_data *data)
{
	struct aplic_irqsrc *src;
	struct aplic_softc *sc;
	uint32_t reg;

	CPU_ZERO(&isrc->isrc_cpu);

	sc = device_get_softc(dev);
	src = (struct aplic_irqsrc *)isrc;

	if (bootverbose)
		device_printf(dev, "configuring irq %d type %x\n", src->irq,
		    src->type);

	switch (src->type) {
	case IRQ_TYPE_LEVEL_HIGH:
		reg = SRC_CFG_SM_LVL_HI;
		break;
	case IRQ_TYPE_LEVEL_LOW:
		reg = SRC_CFG_SM_LVL_LO;
		break;
	case IRQ_TYPE_EDGE_RISING:
		reg = SRC_CFG_SM_EDGE_RSE;
		break;
	case IRQ_TYPE_EDGE_FALLING:
		reg = SRC_CFG_SM_EDGE_FLL;
		break;
	case IRQ_TYPE_EDGE_BOTH:
		reg = SRC_CFG_SM_EDGE_FLL | SRC_CFG_SM_EDGE_RSE;
		break;
	default:
		panic("unknown level");
	};

	aplic_write(sc, APLIC_SRC_CFG(src->irq), reg);

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
	uint32_t reg;

	sc = device_get_softc(dev);
	src = (struct aplic_irqsrc *)isrc;

	/* Disable the interrupt source */
	aplic_write(sc, APLIC_CLRIENUM, src->irq);

	if (CPU_EMPTY(&isrc->isrc_cpu)) {
		cpu = aplic_irq_cpu = intr_irq_next_cpu(aplic_irq_cpu,
		    &all_cpus);
		CPU_SETOF(cpu, &isrc->isrc_cpu);
	} else
		cpu = CPU_FFS(&isrc->isrc_cpu) - 1;

	hartid = riscv_cpu_to_hartid(cpu);

	if (bootverbose)
		device_printf(dev, "Bind irq %d to cpu%d (hart %d)\n", src->irq,
		    cpu, hartid);

	if (sc->has_imsic) {
		if (imsic_alloc_aplic_msi(sc->parent, cpu, src->irq,
		    &src->msi)) {
			device_printf(dev, "MSI allocation for irq %d failed\n",
			    src->irq);
			return (ENXIO);
		}

		if (bootverbose)
			device_printf(dev, "Setting up IRQ %d on CPU %d "
			    "MSI ID %d\n", src->irq, cpu, src->msi);

		reg = hartid << TARGET_HART_ID_S;
		reg |= src->msi << TARGET_EIID_S;
		aplic_write(sc, APLIC_TARGET(src->irq), reg);
	} else {
		reg = sc->hart_indices[cpu] << TARGET_HART_ID_S;
		reg |= (APLIC_INTR_DEF_PRIO << TARGET_IPRIO_S) & TARGET_IPRIO_M;
		aplic_write(sc, APLIC_TARGET(src->irq), reg);
		aplic_write(sc, APLIC_IDC_IDELIVERY(sc, cpu), IDELIVERY_ENABLE);
	}

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
