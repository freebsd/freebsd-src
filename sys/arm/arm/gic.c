/*-
 * Copyright (c) 2011 The FreeBSD Foundation
 * All rights reserved.
 *
 * Developed by Damjan Marion <damjan.marion@gmail.com>
 *
 * Based on OMAP4 GIC code by Ben Gray
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
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

#include "opt_platform.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/module.h>
#include <sys/malloc.h>
#include <sys/rman.h>
#include <sys/pcpu.h>
#include <sys/proc.h>
#include <sys/cpuset.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/smp.h>
#ifdef INTRNG
#include <sys/sched.h>
#endif
#include <machine/bus.h>
#include <machine/intr.h>
#include <machine/smp.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#ifdef INTRNG
#include "pic_if.h"
#endif

#define GIC_DEBUG_SPURIOUS

/* We are using GICv2 register naming */

/* Distributor Registers */
#define GICD_CTLR		0x000			/* v1 ICDDCR */
#define GICD_TYPER		0x004			/* v1 ICDICTR */
#define GICD_IIDR		0x008			/* v1 ICDIIDR */
#define GICD_IGROUPR(n)		(0x0080 + ((n) * 4))	/* v1 ICDISER */
#define GICD_ISENABLER(n)	(0x0100 + ((n) * 4))	/* v1 ICDISER */
#define GICD_ICENABLER(n)	(0x0180 + ((n) * 4))	/* v1 ICDICER */
#define GICD_ISPENDR(n)		(0x0200 + ((n) * 4))	/* v1 ICDISPR */
#define GICD_ICPENDR(n)		(0x0280 + ((n) * 4))	/* v1 ICDICPR */
#define GICD_ICACTIVER(n)	(0x0380 + ((n) * 4))	/* v1 ICDABR */
#define GICD_IPRIORITYR(n)	(0x0400 + ((n) * 4))	/* v1 ICDIPR */
#define GICD_ITARGETSR(n)	(0x0800 + ((n) * 4))	/* v1 ICDIPTR */
#define GICD_ICFGR(n)		(0x0C00 + ((n) * 4))	/* v1 ICDICFR */
#define GICD_SGIR(n)		(0x0F00 + ((n) * 4))	/* v1 ICDSGIR */
#define  GICD_SGI_TARGET_SHIFT	16

/* CPU Registers */
#define GICC_CTLR		0x0000			/* v1 ICCICR */
#define GICC_PMR		0x0004			/* v1 ICCPMR */
#define GICC_BPR		0x0008			/* v1 ICCBPR */
#define GICC_IAR		0x000C			/* v1 ICCIAR */
#define GICC_EOIR		0x0010			/* v1 ICCEOIR */
#define GICC_RPR		0x0014			/* v1 ICCRPR */
#define GICC_HPPIR		0x0018			/* v1 ICCHPIR */
#define GICC_ABPR		0x001C			/* v1 ICCABPR */
#define GICC_IIDR		0x00FC			/* v1 ICCIIDR*/

#define	GIC_FIRST_SGI		 0	/* Irqs 0-15 are SGIs/IPIs. */
#define	GIC_LAST_SGI		15
#define	GIC_FIRST_PPI		16	/* Irqs 16-31 are private (per */
#define	GIC_LAST_PPI		31	/* core) peripheral interrupts. */
#define	GIC_FIRST_SPI		32	/* Irqs 32+ are shared peripherals. */

/* First bit is a polarity bit (0 - low, 1 - high) */
#define GICD_ICFGR_POL_LOW	(0 << 0)
#define GICD_ICFGR_POL_HIGH	(1 << 0)
#define GICD_ICFGR_POL_MASK	0x1
/* Second bit is a trigger bit (0 - level, 1 - edge) */
#define GICD_ICFGR_TRIG_LVL	(0 << 1)
#define GICD_ICFGR_TRIG_EDGE	(1 << 1)
#define GICD_ICFGR_TRIG_MASK	0x2

#ifndef	GIC_DEFAULT_ICFGR_INIT
#define	GIC_DEFAULT_ICFGR_INIT	0x00000000
#endif

#ifdef INTRNG
struct gic_irqsrc {
	struct intr_irqsrc	gi_isrc;
	uint32_t		gi_irq;
	enum intr_polarity	gi_pol;
	enum intr_trigger	gi_trig;
#define GI_FLAG_EARLY_EOI	(1 << 0)
	u_int			gi_flags;
};

static u_int gic_irq_cpu;
static int arm_gic_intr(void *);
static int arm_gic_bind_intr(device_t dev, struct intr_irqsrc *isrc);

#ifdef SMP
static u_int sgi_to_ipi[GIC_LAST_SGI - GIC_FIRST_SGI + 1];
static u_int sgi_first_unused = GIC_FIRST_SGI;
#endif
#endif

#ifdef INTRNG
struct arm_gic_range {
	uint64_t bus;
	uint64_t host;
	uint64_t size;
};

struct arm_gic_devinfo {
	struct ofw_bus_devinfo	obdinfo;
	struct resource_list	rl;
};
#endif

struct arm_gic_softc {
	device_t		gic_dev;
#ifdef INTRNG
	void *			gic_intrhand;
	struct gic_irqsrc *	gic_irqs;
#endif
	struct resource *	gic_res[3];
	bus_space_tag_t		gic_c_bst;
	bus_space_tag_t		gic_d_bst;
	bus_space_handle_t	gic_c_bsh;
	bus_space_handle_t	gic_d_bsh;
	uint8_t			ver;
	struct mtx		mutex;
	uint32_t		nirqs;
#ifdef GIC_DEBUG_SPURIOUS
	uint32_t		last_irq[MAXCPU];
#endif

#ifdef INTRNG
	/* FDT child data */
	pcell_t			addr_cells;
	pcell_t			size_cells;
	int			nranges;
	struct arm_gic_range *	ranges;
#endif
};

#ifdef INTRNG
#define GIC_INTR_ISRC(sc, irq)	(&sc->gic_irqs[irq].gi_isrc)
#endif

static struct resource_spec arm_gic_spec[] = {
	{ SYS_RES_MEMORY,	0,	RF_ACTIVE },	/* Distributor registers */
	{ SYS_RES_MEMORY,	1,	RF_ACTIVE },	/* CPU Interrupt Intf. registers */
#ifdef INTRNG
	{ SYS_RES_IRQ,	  0, RF_ACTIVE | RF_OPTIONAL }, /* Parent interrupt */
#endif
	{ -1, 0 }
};

static u_int arm_gic_map[MAXCPU];

static struct arm_gic_softc *gic_sc = NULL;

#define	gic_c_read_4(_sc, _reg)		\
    bus_space_read_4((_sc)->gic_c_bst, (_sc)->gic_c_bsh, (_reg))
#define	gic_c_write_4(_sc, _reg, _val)		\
    bus_space_write_4((_sc)->gic_c_bst, (_sc)->gic_c_bsh, (_reg), (_val))
#define	gic_d_read_4(_sc, _reg)		\
    bus_space_read_4((_sc)->gic_d_bst, (_sc)->gic_d_bsh, (_reg))
#define	gic_d_write_1(_sc, _reg, _val)		\
    bus_space_write_1((_sc)->gic_d_bst, (_sc)->gic_d_bsh, (_reg), (_val))
#define	gic_d_write_4(_sc, _reg, _val)		\
    bus_space_write_4((_sc)->gic_d_bst, (_sc)->gic_d_bsh, (_reg), (_val))

#ifndef INTRNG
static int gic_config_irq(int irq, enum intr_trigger trig,
    enum intr_polarity pol);
static void gic_post_filter(void *);
#endif

static struct ofw_compat_data compat_data[] = {
	{"arm,gic",		true},	/* Non-standard, used in FreeBSD dts. */
	{"arm,gic-400",		true},
	{"arm,cortex-a15-gic",	true},
	{"arm,cortex-a9-gic",	true},
	{"arm,cortex-a7-gic",	true},
	{"arm,arm11mp-gic",	true},
	{"brcm,brahma-b15-gic",	true},
	{"qcom,msm-qgic2",	true},
	{NULL,			false}
};

static int
arm_gic_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (!ofw_bus_search_compatible(dev, compat_data)->ocd_data)
		return (ENXIO);
	device_set_desc(dev, "ARM Generic Interrupt Controller");
	return (BUS_PROBE_DEFAULT);
}

#ifdef INTRNG
static inline void
gic_irq_unmask(struct arm_gic_softc *sc, u_int irq)
{

	gic_d_write_4(sc, GICD_ISENABLER(irq >> 5), (1UL << (irq & 0x1F)));
}

static inline void
gic_irq_mask(struct arm_gic_softc *sc, u_int irq)
{

	gic_d_write_4(sc, GICD_ICENABLER(irq >> 5), (1UL << (irq & 0x1F)));
}
#endif

static uint8_t
gic_cpu_mask(struct arm_gic_softc *sc)
{
	uint32_t mask;
	int i;

	/* Read the current cpuid mask by reading ITARGETSR{0..7} */
	for (i = 0; i < 8; i++) {
		mask = gic_d_read_4(sc, GICD_ITARGETSR(i));
		if (mask != 0)
			break;
	}
	/* No mask found, assume we are on CPU interface 0 */
	if (mask == 0)
		return (1);

	/* Collect the mask in the lower byte */
	mask |= mask >> 16;
	mask |= mask >> 8;

	return (mask);
}

#ifdef SMP
#ifdef INTRNG
static void
arm_gic_init_secondary(device_t dev)
{
	struct arm_gic_softc *sc = device_get_softc(dev);
	u_int irq, cpu;

	/* Set the mask so we can find this CPU to send it IPIs */
	cpu = PCPU_GET(cpuid);
	arm_gic_map[cpu] = gic_cpu_mask(sc);

	for (irq = 0; irq < sc->nirqs; irq += 4)
		gic_d_write_4(sc, GICD_IPRIORITYR(irq >> 2), 0);

	/* Set all the interrupts to be in Group 0 (secure) */
	for (irq = 0; irq < sc->nirqs; irq += 32) {
		gic_d_write_4(sc, GICD_IGROUPR(irq >> 5), 0);
	}

	/* Enable CPU interface */
	gic_c_write_4(sc, GICC_CTLR, 1);

	/* Set priority mask register. */
	gic_c_write_4(sc, GICC_PMR, 0xff);

	/* Enable interrupt distribution */
	gic_d_write_4(sc, GICD_CTLR, 0x01);

	/* Unmask attached SGI interrupts. */
	for (irq = GIC_FIRST_SGI; irq <= GIC_LAST_SGI; irq++)
		if (intr_isrc_init_on_cpu(GIC_INTR_ISRC(sc, irq), cpu))
			gic_irq_unmask(sc, irq);

	/* Unmask attached PPI interrupts. */
	for (irq = GIC_FIRST_PPI; irq <= GIC_LAST_PPI; irq++)
		if (intr_isrc_init_on_cpu(GIC_INTR_ISRC(sc, irq), cpu))
			gic_irq_unmask(sc, irq);
}
#else
static void
arm_gic_init_secondary(device_t dev)
{
	struct arm_gic_softc *sc = device_get_softc(dev);
	int i;

	/* Set the mask so we can find this CPU to send it IPIs */
	arm_gic_map[PCPU_GET(cpuid)] = gic_cpu_mask(sc);

	for (i = 0; i < sc->nirqs; i += 4)
		gic_d_write_4(sc, GICD_IPRIORITYR(i >> 2), 0);

	/* Set all the interrupts to be in Group 0 (secure) */
	for (i = 0; i < sc->nirqs; i += 32) {
		gic_d_write_4(sc, GICD_IGROUPR(i >> 5), 0);
	}

	/* Enable CPU interface */
	gic_c_write_4(sc, GICC_CTLR, 1);

	/* Set priority mask register. */
	gic_c_write_4(sc, GICC_PMR, 0xff);

	/* Enable interrupt distribution */
	gic_d_write_4(sc, GICD_CTLR, 0x01);

	/*
	 * Activate the timer interrupts: virtual, secure, and non-secure.
	 */
	gic_d_write_4(sc, GICD_ISENABLER(27 >> 5), (1UL << (27 & 0x1F)));
	gic_d_write_4(sc, GICD_ISENABLER(29 >> 5), (1UL << (29 & 0x1F)));
	gic_d_write_4(sc, GICD_ISENABLER(30 >> 5), (1UL << (30 & 0x1F)));
}
#endif /* INTRNG */
#endif /* SMP */

#ifndef INTRNG
int
gic_decode_fdt(phandle_t iparent, pcell_t *intr, int *interrupt,
    int *trig, int *pol)
{
	static u_int num_intr_cells;
	static phandle_t self;
	struct ofw_compat_data *ocd;

	if (self == 0) {
		for (ocd = compat_data; ocd->ocd_str != NULL; ocd++) {
			if (fdt_is_compatible(iparent, ocd->ocd_str)) {
				self = iparent;
				break;
			}
		}
	}
	if (self != iparent)
		return (ENXIO);

	if (num_intr_cells == 0) {
		if (OF_searchencprop(OF_node_from_xref(iparent),
		    "#interrupt-cells", &num_intr_cells,
		    sizeof(num_intr_cells)) == -1) {
			num_intr_cells = 1;
		}
	}

	if (num_intr_cells == 1) {
		*interrupt = fdt32_to_cpu(intr[0]);
		*trig = INTR_TRIGGER_CONFORM;
		*pol = INTR_POLARITY_CONFORM;
	} else {
		if (fdt32_to_cpu(intr[0]) == 0)
			*interrupt = fdt32_to_cpu(intr[1]) + GIC_FIRST_SPI;
		else
			*interrupt = fdt32_to_cpu(intr[1]) + GIC_FIRST_PPI;
		/*
		 * In intr[2], bits[3:0] are trigger type and level flags.
		 *   1 = low-to-high edge triggered
		 *   2 = high-to-low edge triggered
		 *   4 = active high level-sensitive
		 *   8 = active low level-sensitive
		 * The hardware only supports active-high-level or rising-edge
		 * for SPIs
		 */
		if (*interrupt >= GIC_FIRST_SPI &&
		    fdt32_to_cpu(intr[2]) & 0x0a) {
			printf("unsupported trigger/polarity configuration "
			    "0x%02x\n", fdt32_to_cpu(intr[2]) & 0x0f);
		}
		*pol  = INTR_POLARITY_CONFORM;
		if (fdt32_to_cpu(intr[2]) & 0x03)
			*trig = INTR_TRIGGER_EDGE;
		else
			*trig = INTR_TRIGGER_LEVEL;
	}
	return (0);
}
#endif

#ifdef INTRNG
static inline intptr_t
gic_xref(device_t dev)
{
#ifdef FDT
	return (OF_xref_from_node(ofw_bus_get_node(dev)));
#else
	return (0);
#endif
}

static int
arm_gic_register_isrcs(struct arm_gic_softc *sc, uint32_t num)
{
	int error;
	uint32_t irq;
	struct gic_irqsrc *irqs;
	struct intr_irqsrc *isrc;
	const char *name;

	irqs = malloc(num * sizeof(struct gic_irqsrc), M_DEVBUF,
	    M_WAITOK | M_ZERO);

	name = device_get_nameunit(sc->gic_dev);
	for (irq = 0; irq < num; irq++) {
		irqs[irq].gi_irq = irq;
		irqs[irq].gi_pol = INTR_POLARITY_CONFORM;
		irqs[irq].gi_trig = INTR_TRIGGER_CONFORM;

		isrc = &irqs[irq].gi_isrc;
		if (irq <= GIC_LAST_SGI) {
			error = intr_isrc_register(isrc, sc->gic_dev,
			    INTR_ISRCF_IPI, "%s,i%u", name, irq - GIC_FIRST_SGI);
		} else if (irq <= GIC_LAST_PPI) {
			error = intr_isrc_register(isrc, sc->gic_dev,
			    INTR_ISRCF_PPI, "%s,p%u", name, irq - GIC_FIRST_PPI);
		} else {
			error = intr_isrc_register(isrc, sc->gic_dev, 0,
			    "%s,s%u", name, irq - GIC_FIRST_SPI);
		}
		if (error != 0) {
			/* XXX call intr_isrc_deregister() */
			free(irqs, M_DEVBUF);
			return (error);
		}
	}
	sc->gic_irqs = irqs;
	sc->nirqs = num;
	return (0);
}

static int
arm_gic_fill_ranges(phandle_t node, struct arm_gic_softc *sc)
{
	pcell_t host_cells;
	cell_t *base_ranges;
	ssize_t nbase_ranges;
	int i, j, k;

	host_cells = 1;
	OF_getencprop(OF_parent(node), "#address-cells", &host_cells,
	    sizeof(host_cells));
	sc->addr_cells = 2;
	OF_getencprop(node, "#address-cells", &sc->addr_cells,
	    sizeof(sc->addr_cells));
	sc->size_cells = 2;
	OF_getencprop(node, "#size-cells", &sc->size_cells,
	    sizeof(sc->size_cells));

	nbase_ranges = OF_getproplen(node, "ranges");
	if (nbase_ranges < 0)
		return (-1);
	sc->nranges = nbase_ranges / sizeof(cell_t) /
	    (sc->addr_cells + host_cells + sc->size_cells);
	if (sc->nranges == 0)
		return (0);

	sc->ranges = malloc(sc->nranges * sizeof(sc->ranges[0]),
	    M_DEVBUF, M_WAITOK);
	base_ranges = malloc(nbase_ranges, M_DEVBUF, M_WAITOK);
	OF_getencprop(node, "ranges", base_ranges, nbase_ranges);

	for (i = 0, j = 0; i < sc->nranges; i++) {
		sc->ranges[i].bus = 0;
		for (k = 0; k < sc->addr_cells; k++) {
			sc->ranges[i].bus <<= 32;
			sc->ranges[i].bus |= base_ranges[j++];
		}
		sc->ranges[i].host = 0;
		for (k = 0; k < host_cells; k++) {
			sc->ranges[i].host <<= 32;
			sc->ranges[i].host |= base_ranges[j++];
		}
		sc->ranges[i].size = 0;
		for (k = 0; k < sc->size_cells; k++) {
			sc->ranges[i].size <<= 32;
			sc->ranges[i].size |= base_ranges[j++];
		}
	}

	free(base_ranges, M_DEVBUF);
	return (sc->nranges);
}

static bool
arm_gic_add_children(device_t dev)
{
	struct arm_gic_softc *sc;
	struct arm_gic_devinfo *dinfo;
	phandle_t child, node;
	device_t cdev;

	sc = device_get_softc(dev);
	node = ofw_bus_get_node(dev);

	/* If we have no children don't probe for them */
	child = OF_child(node);
	if (child == 0)
		return (false);

	if (arm_gic_fill_ranges(node, sc) < 0) {
		device_printf(dev, "Have a child, but no ranges\n");
		return (false);
	}

	for (; child != 0; child = OF_peer(child)) {
		dinfo = malloc(sizeof(*dinfo), M_DEVBUF, M_WAITOK | M_ZERO);

		if (ofw_bus_gen_setup_devinfo(&dinfo->obdinfo, child) != 0) {
			free(dinfo, M_DEVBUF);
			continue;
		}

		resource_list_init(&dinfo->rl);
		ofw_bus_reg_to_rl(dev, child, sc->addr_cells,
		    sc->size_cells, &dinfo->rl);

		cdev = device_add_child(dev, NULL, -1);
		if (cdev == NULL) {
			device_printf(dev, "<%s>: device_add_child failed\n",
			    dinfo->obdinfo.obd_name);
			resource_list_free(&dinfo->rl);
			ofw_bus_gen_destroy_devinfo(&dinfo->obdinfo);
			free(dinfo, M_DEVBUF);
			continue;
		}
		device_set_ivars(cdev, dinfo);
	}

	return (true);
}
#endif

static int
arm_gic_attach(device_t dev)
{
	struct		arm_gic_softc *sc;
	int		i;
	uint32_t	icciidr, mask, nirqs;
#ifdef INTRNG
	phandle_t	pxref;
	intptr_t	xref = gic_xref(dev);
#endif

	if (gic_sc)
		return (ENXIO);

	sc = device_get_softc(dev);

	if (bus_alloc_resources(dev, arm_gic_spec, sc->gic_res)) {
		device_printf(dev, "could not allocate resources\n");
		return (ENXIO);
	}

	sc->gic_dev = dev;
	gic_sc = sc;

	/* Initialize mutex */
	mtx_init(&sc->mutex, "GIC lock", "", MTX_SPIN);

	/* Distributor Interface */
	sc->gic_d_bst = rman_get_bustag(sc->gic_res[0]);
	sc->gic_d_bsh = rman_get_bushandle(sc->gic_res[0]);

	/* CPU Interface */
	sc->gic_c_bst = rman_get_bustag(sc->gic_res[1]);
	sc->gic_c_bsh = rman_get_bushandle(sc->gic_res[1]);

	/* Disable interrupt forwarding to the CPU interface */
	gic_d_write_4(sc, GICD_CTLR, 0x00);

	/* Get the number of interrupts */
	nirqs = gic_d_read_4(sc, GICD_TYPER);
	nirqs = 32 * ((nirqs & 0x1f) + 1);

#ifdef INTRNG
	if (arm_gic_register_isrcs(sc, nirqs)) {
		device_printf(dev, "could not register irqs\n");
		goto cleanup;
	}
#else
	sc->nirqs = nirqs;

	/* Set up function pointers */
	arm_post_filter = gic_post_filter;
	arm_config_irq = gic_config_irq;
#endif

	icciidr = gic_c_read_4(sc, GICC_IIDR);
	device_printf(dev,"pn 0x%x, arch 0x%x, rev 0x%x, implementer 0x%x irqs %u\n",
			icciidr>>20, (icciidr>>16) & 0xF, (icciidr>>12) & 0xf,
			(icciidr & 0xfff), sc->nirqs);

	/* Set all global interrupts to be level triggered, active low. */
	for (i = 32; i < sc->nirqs; i += 16) {
		gic_d_write_4(sc, GICD_ICFGR(i >> 4), GIC_DEFAULT_ICFGR_INIT);
	}

	/* Disable all interrupts. */
	for (i = 32; i < sc->nirqs; i += 32) {
		gic_d_write_4(sc, GICD_ICENABLER(i >> 5), 0xFFFFFFFF);
	}

	/* Find the current cpu mask */
	mask = gic_cpu_mask(sc);
	/* Set the mask so we can find this CPU to send it IPIs */
	arm_gic_map[PCPU_GET(cpuid)] = mask;
	/* Set all four targets to this cpu */
	mask |= mask << 8;
	mask |= mask << 16;

	for (i = 0; i < sc->nirqs; i += 4) {
		gic_d_write_4(sc, GICD_IPRIORITYR(i >> 2), 0);
		if (i > 32) {
			gic_d_write_4(sc, GICD_ITARGETSR(i >> 2), mask);
		}
	}

	/* Set all the interrupts to be in Group 0 (secure) */
	for (i = 0; i < sc->nirqs; i += 32) {
		gic_d_write_4(sc, GICD_IGROUPR(i >> 5), 0);
	}

	/* Enable CPU interface */
	gic_c_write_4(sc, GICC_CTLR, 1);

	/* Set priority mask register. */
	gic_c_write_4(sc, GICC_PMR, 0xff);

	/* Enable interrupt distribution */
	gic_d_write_4(sc, GICD_CTLR, 0x01);
#ifndef INTRNG
	return (0);
#else
	/*
	 * Now, when everything is initialized, it's right time to
	 * register interrupt controller to interrupt framefork.
	 */
	if (intr_pic_register(dev, xref) != 0) {
		device_printf(dev, "could not register PIC\n");
		goto cleanup;
	}

	/*
	 * Controller is root if:
	 * - doesn't have interrupt parent
	 * - his interrupt parent is this controller
	 */
	pxref = ofw_bus_find_iparent(ofw_bus_get_node(dev));
	if (pxref == 0 || xref == pxref) {
		if (intr_pic_claim_root(dev, xref, arm_gic_intr, sc,
		    GIC_LAST_SGI - GIC_FIRST_SGI + 1) != 0) {
			device_printf(dev, "could not set PIC as a root\n");
			intr_pic_deregister(dev, xref);
			goto cleanup;
		}
	} else {
		if (sc->gic_res[2] == NULL) {
			device_printf(dev,
			    "not root PIC must have defined interrupt\n");
			intr_pic_deregister(dev, xref);
			goto cleanup;
		}
		if (bus_setup_intr(dev, sc->gic_res[2], INTR_TYPE_CLK,
		    arm_gic_intr, NULL, sc, &sc->gic_intrhand)) {
			device_printf(dev, "could not setup irq handler\n");
			intr_pic_deregister(dev, xref);
			goto cleanup;
		}
	}

	OF_device_register_xref(xref, dev);

	/* If we have children probe and attach them */
	if (arm_gic_add_children(dev)) {
		bus_generic_probe(dev);
		return (bus_generic_attach(dev));
	}

	return (0);

cleanup:
	/*
	 * XXX - not implemented arm_gic_detach() should be called !
	 */
	if (sc->gic_irqs != NULL)
		free(sc->gic_irqs, M_DEVBUF);
	bus_release_resources(dev, arm_gic_spec, sc->gic_res);
	return(ENXIO);
#endif
}

#ifdef INTRNG
static struct resource *
arm_gic_alloc_resource(device_t bus, device_t child, int type, int *rid,
    rman_res_t start, rman_res_t end, rman_res_t count, u_int flags)
{
	struct arm_gic_softc *sc;
	struct arm_gic_devinfo *di;
	struct resource_list_entry *rle;
	int j;

	KASSERT(type == SYS_RES_MEMORY, ("Invalid resoure type %x", type));

	sc = device_get_softc(bus);

	/*
	 * Request for the default allocation with a given rid: use resource
	 * list stored in the local device info.
	 */
	if (RMAN_IS_DEFAULT_RANGE(start, end)) {
		if ((di = device_get_ivars(child)) == NULL)
			return (NULL);

		if (type == SYS_RES_IOPORT)
			type = SYS_RES_MEMORY;

		rle = resource_list_find(&di->rl, type, *rid);
		if (rle == NULL) {
			if (bootverbose)
				device_printf(bus, "no default resources for "
				    "rid = %d, type = %d\n", *rid, type);
			return (NULL);
		}
		start = rle->start;
		end = rle->end;
		count = rle->count;
	}

	/* Remap through ranges property */
	for (j = 0; j < sc->nranges; j++) {
		if (start >= sc->ranges[j].bus && end <
		    sc->ranges[j].bus + sc->ranges[j].size) {
			start -= sc->ranges[j].bus;
			start += sc->ranges[j].host;
			end -= sc->ranges[j].bus;
			end += sc->ranges[j].host;
			break;
		}
	}
	if (j == sc->nranges && sc->nranges != 0) {
		if (bootverbose)
			device_printf(bus, "Could not map resource "
			    "%#jx-%#jx\n", (uintmax_t)start, (uintmax_t)end);

		return (NULL);
	}

	return (bus_generic_alloc_resource(bus, child, type, rid, start, end,
	    count, flags));
}

static const struct ofw_bus_devinfo *
arm_gic_ofw_get_devinfo(device_t bus __unused, device_t child)
{
	struct arm_gic_devinfo *di;

	di = device_get_ivars(child);

	return (&di->obdinfo);
}

static int
arm_gic_intr(void *arg)
{
	struct arm_gic_softc *sc = arg;
	struct gic_irqsrc *gi;
	uint32_t irq_active_reg, irq;
	struct trapframe *tf;

	irq_active_reg = gic_c_read_4(sc, GICC_IAR);
	irq = irq_active_reg & 0x3FF;

	/*
	 * 1. We do EOI here because recent read value from active interrupt
	 *    register must be used for it. Another approach is to save this
	 *    value into associated interrupt source.
	 * 2. EOI must be done on same CPU where interrupt has fired. Thus
	 *    we must ensure that interrupted thread does not migrate to
	 *    another CPU.
	 * 3. EOI cannot be delayed by any preemption which could happen on
	 *    critical_exit() used in MI intr code, when interrupt thread is
	 *    scheduled. See next point.
	 * 4. IPI_RENDEZVOUS assumes that no preemption is permitted during
	 *    an action and any use of critical_exit() could break this
	 *    assumption. See comments within smp_rendezvous_action().
	 * 5. We always return FILTER_HANDLED as this is an interrupt
	 *    controller dispatch function. Otherwise, in cascaded interrupt
	 *    case, the whole interrupt subtree would be masked.
	 */

	if (irq >= sc->nirqs) {
#ifdef GIC_DEBUG_SPURIOUS
		device_printf(sc->gic_dev,
		    "Spurious interrupt detected: last irq: %d on CPU%d\n",
		    sc->last_irq[PCPU_GET(cpuid)], PCPU_GET(cpuid));
#endif
		return (FILTER_HANDLED);
	}

	tf = curthread->td_intr_frame;
dispatch_irq:
	gi = sc->gic_irqs + irq;
	/*
	 * Note that GIC_FIRST_SGI is zero and is not used in 'if' statement
	 * as compiler complains that comparing u_int >= 0 is always true.
	 */
	if (irq <= GIC_LAST_SGI) {
#ifdef SMP
		/* Call EOI for all IPI before dispatch. */
		gic_c_write_4(sc, GICC_EOIR, irq_active_reg);
		intr_ipi_dispatch(sgi_to_ipi[gi->gi_irq], tf);
		goto next_irq;
#else
		device_printf(sc->gic_dev, "SGI %u on UP system detected\n",
		    irq - GIC_FIRST_SGI);
		gic_c_write_4(sc, GICC_EOIR, irq_active_reg);
		goto next_irq;
#endif
	}

#ifdef GIC_DEBUG_SPURIOUS
	sc->last_irq[PCPU_GET(cpuid)] = irq;
#endif
	if ((gi->gi_flags & GI_FLAG_EARLY_EOI) == GI_FLAG_EARLY_EOI)
		gic_c_write_4(sc, GICC_EOIR, irq_active_reg);

	if (intr_isrc_dispatch(&gi->gi_isrc, tf) != 0) {
		gic_irq_mask(sc, irq);
		if ((gi->gi_flags & GI_FLAG_EARLY_EOI) != GI_FLAG_EARLY_EOI)
			gic_c_write_4(sc, GICC_EOIR, irq_active_reg);
		device_printf(sc->gic_dev, "Stray irq %u disabled\n", irq);
	}

next_irq:
	arm_irq_memory_barrier(irq);
	irq_active_reg = gic_c_read_4(sc, GICC_IAR);
	irq = irq_active_reg & 0x3FF;
	if (irq < sc->nirqs)
		goto dispatch_irq;

	return (FILTER_HANDLED);
}

static void
gic_config(struct arm_gic_softc *sc, u_int irq, enum intr_trigger trig,
    enum intr_polarity pol)
{
	uint32_t reg;
	uint32_t mask;

	if (irq < GIC_FIRST_SPI)
		return;

	mtx_lock_spin(&sc->mutex);

	reg = gic_d_read_4(sc, GICD_ICFGR(irq >> 4));
	mask = (reg >> 2*(irq % 16)) & 0x3;

	if (pol == INTR_POLARITY_LOW) {
		mask &= ~GICD_ICFGR_POL_MASK;
		mask |= GICD_ICFGR_POL_LOW;
	} else if (pol == INTR_POLARITY_HIGH) {
		mask &= ~GICD_ICFGR_POL_MASK;
		mask |= GICD_ICFGR_POL_HIGH;
	}

	if (trig == INTR_TRIGGER_LEVEL) {
		mask &= ~GICD_ICFGR_TRIG_MASK;
		mask |= GICD_ICFGR_TRIG_LVL;
	} else if (trig == INTR_TRIGGER_EDGE) {
		mask &= ~GICD_ICFGR_TRIG_MASK;
		mask |= GICD_ICFGR_TRIG_EDGE;
	}

	/* Set mask */
	reg = reg & ~(0x3 << 2*(irq % 16));
	reg = reg | (mask << 2*(irq % 16));
	gic_d_write_4(sc, GICD_ICFGR(irq >> 4), reg);

	mtx_unlock_spin(&sc->mutex);
}

static int
gic_bind(struct arm_gic_softc *sc, u_int irq, cpuset_t *cpus)
{
	uint32_t cpu, end, mask;

	end = min(mp_ncpus, 8);
	for (cpu = end; cpu < MAXCPU; cpu++)
		if (CPU_ISSET(cpu, cpus))
			return (EINVAL);

	for (mask = 0, cpu = 0; cpu < end; cpu++)
		if (CPU_ISSET(cpu, cpus))
			mask |= 1 << cpu;

	gic_d_write_1(sc, GICD_ITARGETSR(0) + irq, mask);
	return (0);
}

#ifdef FDT
static int
gic_map_fdt(device_t dev, u_int ncells, pcell_t *cells, u_int *irqp,
    enum intr_polarity *polp, enum intr_trigger *trigp)
{

	if (ncells == 1) {
		*irqp = cells[0];
		*polp = INTR_POLARITY_CONFORM;
		*trigp = INTR_TRIGGER_CONFORM;
		return (0);
	}
	if (ncells == 3) {
		u_int irq, tripol;

		/*
		 * The 1st cell is the interrupt type:
		 *	0 = SPI
		 *	1 = PPI
		 * The 2nd cell contains the interrupt number:
		 *	[0 - 987] for SPI
		 *	[0 -  15] for PPI
		 * The 3rd cell is the flags, encoded as follows:
		 *   bits[3:0] trigger type and level flags
		 *	1 = low-to-high edge triggered
		 *	2 = high-to-low edge triggered
		 *	4 = active high level-sensitive
		 *	8 = active low level-sensitive
		 *   bits[15:8] PPI interrupt cpu mask
		 *	Each bit corresponds to each of the 8 possible cpus
		 *	attached to the GIC.  A bit set to '1' indicated
		 *	the interrupt is wired to that CPU.
		 */
		switch (cells[0]) {
		case 0:
			irq = GIC_FIRST_SPI + cells[1];
			/* SPI irq is checked later. */
			break;
		case 1:
			irq = GIC_FIRST_PPI + cells[1];
			if (irq > GIC_LAST_PPI) {
				device_printf(dev, "unsupported PPI interrupt "
				    "number %u\n", cells[1]);
				return (EINVAL);
			}
			break;
		default:
			device_printf(dev, "unsupported interrupt type "
			    "configuration %u\n", cells[0]);
			return (EINVAL);
		}

		tripol = cells[2] & 0xff;
		if (tripol & 0xf0 || (tripol & 0x0a && cells[0] == 0))
			device_printf(dev, "unsupported trigger/polarity "
			    "configuration 0x%02x\n", tripol);

		*irqp = irq;
		*polp = INTR_POLARITY_CONFORM;
		*trigp = tripol & 0x03 ? INTR_TRIGGER_EDGE : INTR_TRIGGER_LEVEL;
		return (0);
	}
	return (EINVAL);
}
#endif

static int
gic_map_intr(device_t dev, struct intr_map_data *data, u_int *irqp,
    enum intr_polarity *polp, enum intr_trigger *trigp)
{
	u_int irq;
	enum intr_polarity pol;
	enum intr_trigger trig;
	struct arm_gic_softc *sc;

	sc = device_get_softc(dev);
	switch (data->type) {
#ifdef FDT
	case INTR_MAP_DATA_FDT:
		if (gic_map_fdt(dev, data->fdt.ncells, data->fdt.cells, &irq,
		    &pol, &trig) != 0)
			return (EINVAL);
		break;
#endif
	default:
		return (EINVAL);
	}

	if (irq >= sc->nirqs)
		return (EINVAL);
	if (pol != INTR_POLARITY_CONFORM && pol != INTR_POLARITY_LOW &&
	    pol != INTR_POLARITY_HIGH)
		return (EINVAL);
	if (trig != INTR_TRIGGER_CONFORM && trig != INTR_TRIGGER_EDGE &&
	    trig != INTR_TRIGGER_LEVEL)
		return (EINVAL);

	*irqp = irq;
	if (polp != NULL)
		*polp = pol;
	if (trigp != NULL)
		*trigp = trig;
	return (0);
}

static int
arm_gic_map_intr(device_t dev, struct intr_map_data *data,
    struct intr_irqsrc **isrcp)
{
	int error;
	u_int irq;
	struct arm_gic_softc *sc;

	error = gic_map_intr(dev, data, &irq, NULL, NULL);
	if (error == 0) {
		sc = device_get_softc(dev);
		*isrcp = GIC_INTR_ISRC(sc, irq);
	}
	return (error);
}

static int
arm_gic_setup_intr(device_t dev, struct intr_irqsrc *isrc,
    struct resource *res, struct intr_map_data *data)
{
	struct arm_gic_softc *sc = device_get_softc(dev);
	struct gic_irqsrc *gi = (struct gic_irqsrc *)isrc;
	u_int irq;
	enum intr_trigger trig;
	enum intr_polarity pol;

	if (data == NULL)
		return (ENOTSUP);

	/* Get config for resource. */
	if (gic_map_intr(dev, data, &irq, &pol, &trig))
		return (EINVAL);

	if (gi->gi_irq != irq)
		return (EINVAL);

	/* Compare config if this is not first setup. */
	if (isrc->isrc_handlers != 0) {
		if ((pol != INTR_POLARITY_CONFORM && pol != gi->gi_pol) ||
		    (trig != INTR_TRIGGER_CONFORM && trig != gi->gi_trig))
			return (EINVAL);
		else
			return (0);
	}

	if (pol == INTR_POLARITY_CONFORM)
		pol = INTR_POLARITY_LOW;	/* just pick some */
	if (trig == INTR_TRIGGER_CONFORM)
		trig = INTR_TRIGGER_EDGE;	/* just pick some */

	gi->gi_pol = pol;
	gi->gi_trig = trig;
	/* Edge triggered interrupts need an early EOI sent */
	if (gi->gi_pol == INTR_TRIGGER_EDGE)
		gi->gi_flags |= GI_FLAG_EARLY_EOI;

	/*
	 * XXX - In case that per CPU interrupt is going to be enabled in time
	 *       when SMP is already started, we need some IPI call which
	 *       enables it on others CPUs. Further, it's more complicated as
	 *       pic_enable_source() and pic_disable_source() should act on
	 *       per CPU basis only. Thus, it should be solved here somehow.
	 */
	if (isrc->isrc_flags & INTR_ISRCF_PPI)
		CPU_SET(PCPU_GET(cpuid), &isrc->isrc_cpu);

	gic_config(sc, gi->gi_irq, trig, pol);
	arm_gic_bind_intr(dev, isrc);
	return (0);
}

static int
arm_gic_teardown_intr(device_t dev, struct intr_irqsrc *isrc,
    struct resource *res, struct intr_map_data *data)
{
	struct gic_irqsrc *gi = (struct gic_irqsrc *)isrc;

	if (isrc->isrc_handlers == 0) {
		gi->gi_pol = INTR_POLARITY_CONFORM;
		gi->gi_trig = INTR_TRIGGER_CONFORM;
	}
	return (0);
}

static void
arm_gic_enable_intr(device_t dev, struct intr_irqsrc *isrc)
{
	struct arm_gic_softc *sc = device_get_softc(dev);
	struct gic_irqsrc *gi = (struct gic_irqsrc *)isrc;

	arm_irq_memory_barrier(gi->gi_irq);
	gic_irq_unmask(sc, gi->gi_irq);
}

static void
arm_gic_disable_intr(device_t dev, struct intr_irqsrc *isrc)
{
	struct arm_gic_softc *sc = device_get_softc(dev);
	struct gic_irqsrc *gi = (struct gic_irqsrc *)isrc;

	gic_irq_mask(sc, gi->gi_irq);
}

static void
arm_gic_pre_ithread(device_t dev, struct intr_irqsrc *isrc)
{
	struct arm_gic_softc *sc = device_get_softc(dev);
	struct gic_irqsrc *gi = (struct gic_irqsrc *)isrc;

	arm_gic_disable_intr(dev, isrc);
	gic_c_write_4(sc, GICC_EOIR, gi->gi_irq);
}

static void
arm_gic_post_ithread(device_t dev, struct intr_irqsrc *isrc)
{

	arm_irq_memory_barrier(0);
	arm_gic_enable_intr(dev, isrc);
}

static void
arm_gic_post_filter(device_t dev, struct intr_irqsrc *isrc)
{
	struct arm_gic_softc *sc = device_get_softc(dev);
	struct gic_irqsrc *gi = (struct gic_irqsrc *)isrc;

        /* EOI for edge-triggered done earlier. */
	if ((gi->gi_flags & GI_FLAG_EARLY_EOI) == GI_FLAG_EARLY_EOI)
		return;

	arm_irq_memory_barrier(0);
	gic_c_write_4(sc, GICC_EOIR, gi->gi_irq);
}

static int
arm_gic_bind_intr(device_t dev, struct intr_irqsrc *isrc)
{
	struct arm_gic_softc *sc = device_get_softc(dev);
	struct gic_irqsrc *gi = (struct gic_irqsrc *)isrc;

	if (gi->gi_irq < GIC_FIRST_SPI)
		return (EINVAL);

	if (CPU_EMPTY(&isrc->isrc_cpu)) {
		gic_irq_cpu = intr_irq_next_cpu(gic_irq_cpu, &all_cpus);
		CPU_SETOF(gic_irq_cpu, &isrc->isrc_cpu);
	}
	return (gic_bind(sc, gi->gi_irq, &isrc->isrc_cpu));
}

#ifdef SMP
static void
arm_gic_ipi_send(device_t dev, struct intr_irqsrc *isrc, cpuset_t cpus,
    u_int ipi)
{
	struct arm_gic_softc *sc = device_get_softc(dev);
	struct gic_irqsrc *gi = (struct gic_irqsrc *)isrc;
	uint32_t val = 0, i;

	for (i = 0; i < MAXCPU; i++)
		if (CPU_ISSET(i, &cpus))
			val |= arm_gic_map[i] << GICD_SGI_TARGET_SHIFT;

	gic_d_write_4(sc, GICD_SGIR(0), val | gi->gi_irq);
}

static int
arm_gic_ipi_setup(device_t dev, u_int ipi, struct intr_irqsrc **isrcp)
{
	struct intr_irqsrc *isrc;
	struct arm_gic_softc *sc = device_get_softc(dev);

	if (sgi_first_unused > GIC_LAST_SGI)
		return (ENOSPC);

	isrc = GIC_INTR_ISRC(sc, sgi_first_unused);
	sgi_to_ipi[sgi_first_unused++] = ipi;

	CPU_SET(PCPU_GET(cpuid), &isrc->isrc_cpu);

	*isrcp = isrc;
	return (0);
}
#endif
#else
static int
arm_gic_next_irq(struct arm_gic_softc *sc, int last_irq)
{
	uint32_t active_irq;

	active_irq = gic_c_read_4(sc, GICC_IAR);

	/*
	 * Immediatly EOIR the SGIs, because doing so requires the other
	 * bits (ie CPU number), not just the IRQ number, and we do not
	 * have this information later.
	 */
	if ((active_irq & 0x3ff) <= GIC_LAST_SGI)
		gic_c_write_4(sc, GICC_EOIR, active_irq);
	active_irq &= 0x3FF;

	if (active_irq == 0x3FF) {
		if (last_irq == -1)
			device_printf(sc->gic_dev,
			    "Spurious interrupt detected\n");
		return -1;
	}

	return active_irq;
}

static int
arm_gic_config(device_t dev, int irq, enum intr_trigger trig,
    enum intr_polarity pol)
{
	struct arm_gic_softc *sc = device_get_softc(dev);
	uint32_t reg;
	uint32_t mask;

	/* Function is public-accessible, so validate input arguments */
	if ((irq < 0) || (irq >= sc->nirqs))
		goto invalid_args;
	if ((trig != INTR_TRIGGER_EDGE) && (trig != INTR_TRIGGER_LEVEL) &&
	    (trig != INTR_TRIGGER_CONFORM))
		goto invalid_args;
	if ((pol != INTR_POLARITY_HIGH) && (pol != INTR_POLARITY_LOW) &&
	    (pol != INTR_POLARITY_CONFORM))
		goto invalid_args;

	mtx_lock_spin(&sc->mutex);

	reg = gic_d_read_4(sc, GICD_ICFGR(irq >> 4));
	mask = (reg >> 2*(irq % 16)) & 0x3;

	if (pol == INTR_POLARITY_LOW) {
		mask &= ~GICD_ICFGR_POL_MASK;
		mask |= GICD_ICFGR_POL_LOW;
	} else if (pol == INTR_POLARITY_HIGH) {
		mask &= ~GICD_ICFGR_POL_MASK;
		mask |= GICD_ICFGR_POL_HIGH;
	}

	if (trig == INTR_TRIGGER_LEVEL) {
		mask &= ~GICD_ICFGR_TRIG_MASK;
		mask |= GICD_ICFGR_TRIG_LVL;
	} else if (trig == INTR_TRIGGER_EDGE) {
		mask &= ~GICD_ICFGR_TRIG_MASK;
		mask |= GICD_ICFGR_TRIG_EDGE;
	}

	/* Set mask */
	reg = reg & ~(0x3 << 2*(irq % 16));
	reg = reg | (mask << 2*(irq % 16));
	gic_d_write_4(sc, GICD_ICFGR(irq >> 4), reg);

	mtx_unlock_spin(&sc->mutex);

	return (0);

invalid_args:
	device_printf(dev, "gic_config_irg, invalid parameters\n");
	return (EINVAL);
}


static void
arm_gic_mask(device_t dev, int irq)
{
	struct arm_gic_softc *sc = device_get_softc(dev);

	gic_d_write_4(sc, GICD_ICENABLER(irq >> 5), (1UL << (irq & 0x1F)));
	gic_c_write_4(sc, GICC_EOIR, irq); /* XXX - not allowed */
}

static void
arm_gic_unmask(device_t dev, int irq)
{
	struct arm_gic_softc *sc = device_get_softc(dev);

	if (irq > GIC_LAST_SGI)
		arm_irq_memory_barrier(irq);

	gic_d_write_4(sc, GICD_ISENABLER(irq >> 5), (1UL << (irq & 0x1F)));
}

#ifdef SMP
static void
arm_gic_ipi_send(device_t dev, cpuset_t cpus, u_int ipi)
{
	struct arm_gic_softc *sc = device_get_softc(dev);
	uint32_t val = 0, i;

	for (i = 0; i < MAXCPU; i++)
		if (CPU_ISSET(i, &cpus))
			val |= arm_gic_map[i] << GICD_SGI_TARGET_SHIFT;

	gic_d_write_4(sc, GICD_SGIR(0), val | ipi);
}

static int
arm_gic_ipi_read(device_t dev, int i)
{

	if (i != -1) {
		/*
		 * The intr code will automagically give the frame pointer
		 * if the interrupt argument is 0.
		 */
		if ((unsigned int)i > 16)
			return (0);
		return (i);
	}

	return (0x3ff);
}

static void
arm_gic_ipi_clear(device_t dev, int ipi)
{
	/* no-op */
}
#endif

static void
gic_post_filter(void *arg)
{
	struct arm_gic_softc *sc = gic_sc;
	uintptr_t irq = (uintptr_t) arg;

	if (irq > GIC_LAST_SGI)
		arm_irq_memory_barrier(irq);
	gic_c_write_4(sc, GICC_EOIR, irq);
}

static int
gic_config_irq(int irq, enum intr_trigger trig, enum intr_polarity pol)
{

	return (arm_gic_config(gic_sc->gic_dev, irq, trig, pol));
}

void
arm_mask_irq(uintptr_t nb)
{

	arm_gic_mask(gic_sc->gic_dev, nb);
}

void
arm_unmask_irq(uintptr_t nb)
{

	arm_gic_unmask(gic_sc->gic_dev, nb);
}

int
arm_get_next_irq(int last_irq)
{

	return (arm_gic_next_irq(gic_sc, last_irq));
}

#ifdef SMP
void
intr_pic_init_secondary(void)
{

	arm_gic_init_secondary(gic_sc->gic_dev);
}

void
pic_ipi_send(cpuset_t cpus, u_int ipi)
{

	arm_gic_ipi_send(gic_sc->gic_dev, cpus, ipi);
}

int
pic_ipi_read(int i)
{

	return (arm_gic_ipi_read(gic_sc->gic_dev, i));
}

void
pic_ipi_clear(int ipi)
{

	arm_gic_ipi_clear(gic_sc->gic_dev, ipi);
}
#endif
#endif /* INTRNG */

static device_method_t arm_gic_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		arm_gic_probe),
	DEVMETHOD(device_attach,	arm_gic_attach),

#ifdef INTRNG
	/* Bus interface */
	DEVMETHOD(bus_add_child,	bus_generic_add_child),
	DEVMETHOD(bus_alloc_resource,	arm_gic_alloc_resource),
	DEVMETHOD(bus_release_resource,	bus_generic_release_resource),
	DEVMETHOD(bus_activate_resource,bus_generic_activate_resource),

	/* ofw_bus interface */
	DEVMETHOD(ofw_bus_get_devinfo,	arm_gic_ofw_get_devinfo),
	DEVMETHOD(ofw_bus_get_compat,	ofw_bus_gen_get_compat),
	DEVMETHOD(ofw_bus_get_model,	ofw_bus_gen_get_model),
	DEVMETHOD(ofw_bus_get_name,	ofw_bus_gen_get_name),
	DEVMETHOD(ofw_bus_get_node,	ofw_bus_gen_get_node),
	DEVMETHOD(ofw_bus_get_type,	ofw_bus_gen_get_type),

	/* Interrupt controller interface */
	DEVMETHOD(pic_disable_intr,	arm_gic_disable_intr),
	DEVMETHOD(pic_enable_intr,	arm_gic_enable_intr),
	DEVMETHOD(pic_map_intr,		arm_gic_map_intr),
	DEVMETHOD(pic_setup_intr,	arm_gic_setup_intr),
	DEVMETHOD(pic_teardown_intr,	arm_gic_teardown_intr),
	DEVMETHOD(pic_post_filter,	arm_gic_post_filter),
	DEVMETHOD(pic_post_ithread,	arm_gic_post_ithread),
	DEVMETHOD(pic_pre_ithread,	arm_gic_pre_ithread),
#ifdef SMP
	DEVMETHOD(pic_bind_intr,	arm_gic_bind_intr),
	DEVMETHOD(pic_init_secondary,	arm_gic_init_secondary),
	DEVMETHOD(pic_ipi_send,		arm_gic_ipi_send),
	DEVMETHOD(pic_ipi_setup,	arm_gic_ipi_setup),
#endif
#endif
	{ 0, 0 }
};

static driver_t arm_gic_driver = {
	"gic",
	arm_gic_methods,
	sizeof(struct arm_gic_softc),
};

static devclass_t arm_gic_devclass;

EARLY_DRIVER_MODULE(gic, simplebus, arm_gic_driver, arm_gic_devclass, 0, 0,
    BUS_PASS_INTERRUPT + BUS_PASS_ORDER_MIDDLE);
EARLY_DRIVER_MODULE(gic, ofwbus, arm_gic_driver, arm_gic_devclass, 0, 0,
    BUS_PASS_INTERRUPT + BUS_PASS_ORDER_MIDDLE);

#ifdef INTRNG
/*
 * GICv2m support -- the GICv2 MSI/MSI-X controller.
 */

#define	GICV2M_MSI_TYPER	0x008
#define	 MSI_TYPER_SPI_BASE(x)	(((x) >> 16) & 0x3ff)
#define	 MSI_TYPER_SPI_COUNT(x)	(((x) >> 0) & 0x3ff)
#define	GICv2M_MSI_SETSPI_NS	0x040
#define	GICV2M_MSI_IIDR		0xFCC

struct arm_gicv2m_softc {
	struct resource	*sc_mem;
	struct mtx	sc_mutex;
	u_int		sc_spi_start;
	u_int		sc_spi_count;
	u_int		sc_spi_offset;
};

static struct ofw_compat_data gicv2m_compat_data[] = {
	{"arm,gic-v2m-frame",	true},
	{NULL,			false}
};

static int
arm_gicv2m_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (!ofw_bus_search_compatible(dev, gicv2m_compat_data)->ocd_data)
		return (ENXIO);

	device_set_desc(dev, "ARM Generic Interrupt Controller MSI/MSIX");
	return (BUS_PROBE_DEFAULT);
}

static int
arm_gicv2m_attach(device_t dev)
{
	struct arm_gicv2m_softc *sc;
	uint32_t typer;
	int rid;

	sc = device_get_softc(dev);

	rid = 0;
	sc->sc_mem = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	if (sc->sc_mem == NULL) {
		device_printf(dev, "Unable to allocate resources\n");
		return (ENXIO);
	}

	typer = bus_read_4(sc->sc_mem, GICV2M_MSI_TYPER);
	sc->sc_spi_start = MSI_TYPER_SPI_BASE(typer);
	sc->sc_spi_count = MSI_TYPER_SPI_COUNT(typer);

	mtx_init(&sc->sc_mutex, "GICv2m lock", "", MTX_DEF);

	if (bootverbose)
		device_printf(dev, "using spi %u to %u\n", sc->sc_spi_start,
		    sc->sc_spi_start + sc->sc_spi_count - 1);

	return (0);
}

static device_method_t arm_gicv2m_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		arm_gicv2m_probe),
	DEVMETHOD(device_attach,	arm_gicv2m_attach),

	/* End */
	DEVMETHOD_END
};

DEFINE_CLASS_0(gicv2m, arm_gicv2m_driver, arm_gicv2m_methods,
    sizeof(struct arm_gicv2m_softc));

static devclass_t arm_gicv2m_devclass;

EARLY_DRIVER_MODULE(gicv2m, gic, arm_gicv2m_driver,
    arm_gicv2m_devclass, 0, 0, BUS_PASS_INTERRUPT + BUS_PASS_ORDER_MIDDLE);
#endif
