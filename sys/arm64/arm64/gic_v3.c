/*-
 * Copyright (c) 2015-2016 The FreeBSD Foundation
 *
 * This software was developed by Andrew Turner under
 * the sponsorship of the FreeBSD Foundation.
 *
 * This software was developed by Semihalf under
 * the sponsorship of the FreeBSD Foundation.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "opt_acpi.h"
#include "opt_platform.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bitstring.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/rman.h>
#include <sys/pcpu.h>
#include <sys/proc.h>
#include <sys/cpuset.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/smp.h>
#include <sys/interrupt.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/intr.h>

#ifdef FDT
#include <dev/fdt/fdt_intr.h>
#include <dev/ofw/ofw_bus_subr.h>
#endif

#ifdef DEV_ACPI
#include <contrib/dev/acpica/include/acpi.h>
#include <dev/acpica/acpivar.h>
#endif

#include "gic_if.h"
#include "pic_if.h"
#include "msi_if.h"

#include <arm/arm/gic_common.h>
#include "gic_v3_reg.h"
#include "gic_v3_var.h"

static bus_print_child_t gic_v3_print_child;
static bus_get_domain_t gic_v3_get_domain;
static bus_read_ivar_t gic_v3_read_ivar;
static bus_write_ivar_t gic_v3_write_ivar;
static bus_alloc_resource_t gic_v3_alloc_resource;

static pic_disable_intr_t gic_v3_disable_intr;
static pic_enable_intr_t gic_v3_enable_intr;
static pic_map_intr_t gic_v3_map_intr;
static pic_setup_intr_t gic_v3_setup_intr;
static pic_teardown_intr_t gic_v3_teardown_intr;
static pic_post_filter_t gic_v3_post_filter;
static pic_post_ithread_t gic_v3_post_ithread;
static pic_pre_ithread_t gic_v3_pre_ithread;
static pic_bind_intr_t gic_v3_bind_intr;
#ifdef SMP
static pic_init_secondary_t gic_v3_init_secondary;
static pic_ipi_send_t gic_v3_ipi_send;
static pic_ipi_setup_t gic_v3_ipi_setup;
#endif

static gic_reserve_msi_range_t gic_v3_reserve_msi_range;
static gic_alloc_msi_t gic_v3_gic_alloc_msi;
static gic_release_msi_t gic_v3_gic_release_msi;
static gic_alloc_msix_t gic_v3_gic_alloc_msix;
static gic_release_msix_t gic_v3_gic_release_msix;

static msi_alloc_msi_t gic_v3_alloc_msi;
static msi_release_msi_t gic_v3_release_msi;
static msi_alloc_msix_t gic_v3_alloc_msix;
static msi_release_msix_t gic_v3_release_msix;
static msi_map_msi_t gic_v3_map_msi;

static u_int gic_irq_cpu;
#ifdef SMP
static u_int sgi_to_ipi[GIC_LAST_SGI - GIC_FIRST_SGI + 1];
static u_int sgi_first_unused = GIC_FIRST_SGI;
#endif

static device_method_t gic_v3_methods[] = {
	/* Device interface */
	DEVMETHOD(device_detach,	gic_v3_detach),

	/* Bus interface */
	DEVMETHOD(bus_print_child,	gic_v3_print_child),
	DEVMETHOD(bus_get_domain,	gic_v3_get_domain),
	DEVMETHOD(bus_read_ivar,	gic_v3_read_ivar),
	DEVMETHOD(bus_write_ivar,	gic_v3_write_ivar),
	DEVMETHOD(bus_alloc_resource,	gic_v3_alloc_resource),
	DEVMETHOD(bus_activate_resource, bus_generic_activate_resource),

	/* Interrupt controller interface */
	DEVMETHOD(pic_disable_intr,	gic_v3_disable_intr),
	DEVMETHOD(pic_enable_intr,	gic_v3_enable_intr),
	DEVMETHOD(pic_map_intr,		gic_v3_map_intr),
	DEVMETHOD(pic_setup_intr,	gic_v3_setup_intr),
	DEVMETHOD(pic_teardown_intr,	gic_v3_teardown_intr),
	DEVMETHOD(pic_post_filter,	gic_v3_post_filter),
	DEVMETHOD(pic_post_ithread,	gic_v3_post_ithread),
	DEVMETHOD(pic_pre_ithread,	gic_v3_pre_ithread),
#ifdef SMP
	DEVMETHOD(pic_bind_intr,	gic_v3_bind_intr),
	DEVMETHOD(pic_init_secondary,	gic_v3_init_secondary),
	DEVMETHOD(pic_ipi_send,		gic_v3_ipi_send),
	DEVMETHOD(pic_ipi_setup,	gic_v3_ipi_setup),
#endif

	/* MSI/MSI-X */
	DEVMETHOD(msi_alloc_msi,        gic_v3_alloc_msi),
	DEVMETHOD(msi_release_msi,      gic_v3_release_msi),
	DEVMETHOD(msi_alloc_msix,       gic_v3_alloc_msix),
	DEVMETHOD(msi_release_msix,     gic_v3_release_msix),
	DEVMETHOD(msi_map_msi,          gic_v3_map_msi),

	/* GIC */
	DEVMETHOD(gic_reserve_msi_range, gic_v3_reserve_msi_range),
	DEVMETHOD(gic_alloc_msi,	gic_v3_gic_alloc_msi),
	DEVMETHOD(gic_release_msi,	gic_v3_gic_release_msi),
	DEVMETHOD(gic_alloc_msix,	gic_v3_gic_alloc_msix),
	DEVMETHOD(gic_release_msix,	gic_v3_gic_release_msix),

	/* End */
	DEVMETHOD_END
};

DEFINE_CLASS_0(gic, gic_v3_driver, gic_v3_methods,
    sizeof(struct gic_v3_softc));

/*
 * Driver-specific definitions.
 */
MALLOC_DEFINE(M_GIC_V3, "GICv3", GIC_V3_DEVSTR);

/*
 * Helper functions and definitions.
 */
/* Destination registers, either Distributor or Re-Distributor */
enum gic_v3_xdist {
	DIST = 0,
	REDIST,
};

struct gic_v3_irqsrc {
	struct intr_irqsrc	gi_isrc;
	uint32_t		gi_irq;
	enum intr_polarity	gi_pol;
	enum intr_trigger	gi_trig;
#define GI_FLAG_MSI		(1 << 1) /* This interrupt source should only */
					 /* be used for MSI/MSI-X interrupts */
#define GI_FLAG_MSI_USED	(1 << 2) /* This irq is already allocated */
					 /* for a MSI/MSI-X interrupt */
	u_int			gi_flags;
};

/* Helper routines starting with gic_v3_ */
static int gic_v3_dist_init(struct gic_v3_softc *);
static int gic_v3_redist_alloc(struct gic_v3_softc *);
static int gic_v3_redist_find(struct gic_v3_softc *);
static int gic_v3_redist_init(struct gic_v3_softc *);
static int gic_v3_cpu_init(struct gic_v3_softc *);
static void gic_v3_wait_for_rwp(struct gic_v3_softc *, enum gic_v3_xdist);

/* A sequence of init functions for primary (boot) CPU */
typedef int (*gic_v3_initseq_t) (struct gic_v3_softc *);
/* Primary CPU initialization sequence */
static gic_v3_initseq_t gic_v3_primary_init[] = {
	gic_v3_dist_init,
	gic_v3_redist_alloc,
	gic_v3_redist_init,
	gic_v3_cpu_init,
	NULL
};

#ifdef SMP
/* Secondary CPU initialization sequence */
static gic_v3_initseq_t gic_v3_secondary_init[] = {
	gic_v3_redist_init,
	gic_v3_cpu_init,
	NULL
};
#endif

uint32_t
gic_r_read_4(device_t dev, bus_size_t offset)
{
	struct gic_v3_softc *sc;
	struct resource *rdist;

	sc = device_get_softc(dev);
	rdist = sc->gic_redists.pcpu[PCPU_GET(cpuid)].res;
	offset += sc->gic_redists.pcpu[PCPU_GET(cpuid)].offset;
	return (bus_read_4(rdist, offset));
}

uint64_t
gic_r_read_8(device_t dev, bus_size_t offset)
{
	struct gic_v3_softc *sc;
	struct resource *rdist;

	sc = device_get_softc(dev);
	rdist = sc->gic_redists.pcpu[PCPU_GET(cpuid)].res;
	offset += sc->gic_redists.pcpu[PCPU_GET(cpuid)].offset;
	return (bus_read_8(rdist, offset));
}

void
gic_r_write_4(device_t dev, bus_size_t offset, uint32_t val)
{
	struct gic_v3_softc *sc;
	struct resource *rdist;

	sc = device_get_softc(dev);
	rdist = sc->gic_redists.pcpu[PCPU_GET(cpuid)].res;
	offset += sc->gic_redists.pcpu[PCPU_GET(cpuid)].offset;
	bus_write_4(rdist, offset, val);
}

void
gic_r_write_8(device_t dev, bus_size_t offset, uint64_t val)
{
	struct gic_v3_softc *sc;
	struct resource *rdist;

	sc = device_get_softc(dev);
	rdist = sc->gic_redists.pcpu[PCPU_GET(cpuid)].res;
	offset += sc->gic_redists.pcpu[PCPU_GET(cpuid)].offset;
	bus_write_8(rdist, offset, val);
}

static void
gic_v3_reserve_msi_range(device_t dev, u_int start, u_int count)
{
	struct gic_v3_softc *sc;
	int i;

	sc = device_get_softc(dev);

	KASSERT((start + count) < sc->gic_nirqs,
	    ("%s: Trying to allocate too many MSI IRQs: %d + %d > %d", __func__,
	    start, count, sc->gic_nirqs));
	for (i = 0; i < count; i++) {
		KASSERT(sc->gic_irqs[start + i].gi_isrc.isrc_handlers == 0,
		    ("%s: MSI interrupt %d already has a handler", __func__,
		    count + i));
		KASSERT(sc->gic_irqs[start + i].gi_pol == INTR_POLARITY_CONFORM,
		    ("%s: MSI interrupt %d already has a polarity", __func__,
		    count + i));
		KASSERT(sc->gic_irqs[start + i].gi_trig == INTR_TRIGGER_CONFORM,
		    ("%s: MSI interrupt %d already has a trigger", __func__,
		    count + i));
		sc->gic_irqs[start + i].gi_pol = INTR_POLARITY_HIGH;
		sc->gic_irqs[start + i].gi_trig = INTR_TRIGGER_EDGE;
		sc->gic_irqs[start + i].gi_flags |= GI_FLAG_MSI;
	}
}

/*
 * Device interface.
 */
int
gic_v3_attach(device_t dev)
{
	struct gic_v3_softc *sc;
	gic_v3_initseq_t *init_func;
	uint32_t typer;
	int rid;
	int err;
	size_t i;
	u_int irq;
	const char *name;

	sc = device_get_softc(dev);
	sc->gic_registered = FALSE;
	sc->dev = dev;
	err = 0;

	/* Initialize mutex */
	mtx_init(&sc->gic_mtx, "GICv3 lock", NULL, MTX_SPIN);

	/*
	 * Allocate array of struct resource.
	 * One entry for Distributor and all remaining for Re-Distributor.
	 */
	sc->gic_res = malloc(
	    sizeof(*sc->gic_res) * (sc->gic_redists.nregions + 1),
	    M_GIC_V3, M_WAITOK);

	/* Now allocate corresponding resources */
	for (i = 0, rid = 0; i < (sc->gic_redists.nregions + 1); i++, rid++) {
		sc->gic_res[rid] = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
		    &rid, RF_ACTIVE);
		if (sc->gic_res[rid] == NULL)
			return (ENXIO);
	}

	/*
	 * Distributor interface
	 */
	sc->gic_dist = sc->gic_res[0];

	/*
	 * Re-Dristributor interface
	 */
	/* Allocate space under region descriptions */
	sc->gic_redists.regions = malloc(
	    sizeof(*sc->gic_redists.regions) * sc->gic_redists.nregions,
	    M_GIC_V3, M_WAITOK);

	/* Fill-up bus_space information for each region. */
	for (i = 0, rid = 1; i < sc->gic_redists.nregions; i++, rid++)
		sc->gic_redists.regions[i] = sc->gic_res[rid];

	/* Get the number of supported SPI interrupts */
	typer = gic_d_read(sc, 4, GICD_TYPER);
	sc->gic_nirqs = GICD_TYPER_I_NUM(typer);
	if (sc->gic_nirqs > GIC_I_NUM_MAX)
		sc->gic_nirqs = GIC_I_NUM_MAX;

	sc->gic_irqs = malloc(sizeof(*sc->gic_irqs) * sc->gic_nirqs,
	    M_GIC_V3, M_WAITOK | M_ZERO);
	name = device_get_nameunit(dev);
	for (irq = 0; irq < sc->gic_nirqs; irq++) {
		struct intr_irqsrc *isrc;

		sc->gic_irqs[irq].gi_irq = irq;
		sc->gic_irqs[irq].gi_pol = INTR_POLARITY_CONFORM;
		sc->gic_irqs[irq].gi_trig = INTR_TRIGGER_CONFORM;

		isrc = &sc->gic_irqs[irq].gi_isrc;
		if (irq <= GIC_LAST_SGI) {
			err = intr_isrc_register(isrc, sc->dev,
			    INTR_ISRCF_IPI, "%s,i%u", name, irq - GIC_FIRST_SGI);
		} else if (irq <= GIC_LAST_PPI) {
			err = intr_isrc_register(isrc, sc->dev,
			    INTR_ISRCF_PPI, "%s,p%u", name, irq - GIC_FIRST_PPI);
		} else {
			err = intr_isrc_register(isrc, sc->dev, 0,
			    "%s,s%u", name, irq - GIC_FIRST_SPI);
		}
		if (err != 0) {
			/* XXX call intr_isrc_deregister() */
			free(sc->gic_irqs, M_DEVBUF);
			return (err);
		}
	}

	mtx_init(&sc->gic_mbi_mtx, "GICv3 mbi lock", NULL, MTX_DEF);
	if (sc->gic_mbi_start > 0) {
		if (!sc->gic_mbi_end) {
			/*
			 * This is to address SPI based msi ranges, where
			 * SPI range is not specified in ACPI
			 */
			sc->gic_mbi_end = sc->gic_nirqs - 1;
		}
		gic_v3_reserve_msi_range(dev, sc->gic_mbi_start,
		    sc->gic_mbi_end - sc->gic_mbi_start);

		if (bootverbose) {
			device_printf(dev, "using spi %u to %u\n", sc->gic_mbi_start,
					sc->gic_mbi_end);
		}
	}

	/*
	 * Read the Peripheral ID2 register. This is an implementation
	 * defined register, but seems to be implemented in all GICv3
	 * parts and Linux expects it to be there.
	 */
	sc->gic_pidr2 = gic_d_read(sc, 4, GICD_PIDR2);

	/* Get the number of supported interrupt identifier bits */
	sc->gic_idbits = GICD_TYPER_IDBITS(typer);

	if (bootverbose) {
		device_printf(dev, "SPIs: %u, IDs: %u\n",
		    sc->gic_nirqs, (1 << sc->gic_idbits) - 1);
	}

	/* Train init sequence for boot CPU */
	for (init_func = gic_v3_primary_init; *init_func != NULL; init_func++) {
		err = (*init_func)(sc);
		if (err != 0)
			return (err);
	}

	return (0);
}

int
gic_v3_detach(device_t dev)
{
	struct gic_v3_softc *sc;
	int rid;

	sc = device_get_softc(dev);

	if (device_is_attached(dev)) {
		/*
		 * XXX: We should probably deregister PIC
		 */
		if (sc->gic_registered)
			panic("Trying to detach registered PIC");
	}
	for (rid = 0; rid < (sc->gic_redists.nregions + 1); rid++)
		bus_release_resource(dev, SYS_RES_MEMORY, rid, sc->gic_res[rid]);

	free(sc->gic_redists.pcpu, M_GIC_V3);

	free(sc->ranges, M_GIC_V3);
	free(sc->gic_res, M_GIC_V3);
	free(sc->gic_redists.regions, M_GIC_V3);

	return (0);
}

static int
gic_v3_print_child(device_t bus, device_t child)
{
	struct resource_list *rl;
	int retval = 0;

	rl = BUS_GET_RESOURCE_LIST(bus, child);
	KASSERT(rl != NULL, ("%s: No resource list", __func__));
	retval += bus_print_child_header(bus, child);
	retval += resource_list_print_type(rl, "mem", SYS_RES_MEMORY, "%#jx");
	retval += bus_print_child_footer(bus, child);

	return (retval);
}

static int
gic_v3_get_domain(device_t dev, device_t child, int *domain)
{
	struct gic_v3_devinfo *di;

	di = device_get_ivars(child);
	if (di->gic_domain < 0)
		return (ENOENT);

	*domain = di->gic_domain;
	return (0);
}

static int
gic_v3_read_ivar(device_t dev, device_t child, int which, uintptr_t *result)
{
	struct gic_v3_softc *sc;
	struct gic_v3_devinfo *di;

	sc = device_get_softc(dev);

	switch (which) {
	case GICV3_IVAR_NIRQS:
		*result = (intr_nirq - sc->gic_nirqs) / sc->gic_nchildren;
		return (0);
	case GICV3_IVAR_REDIST:
		*result = (uintptr_t)&sc->gic_redists.pcpu[PCPU_GET(cpuid)];
		return (0);
	case GICV3_IVAR_SUPPORT_LPIS:
		*result =
		    (gic_d_read(sc, 4, GICD_TYPER) & GICD_TYPER_LPIS) != 0;
		return (0);
	case GIC_IVAR_HW_REV:
		KASSERT(
		    GICR_PIDR2_ARCH(sc->gic_pidr2) == GICR_PIDR2_ARCH_GICv3 ||
		    GICR_PIDR2_ARCH(sc->gic_pidr2) == GICR_PIDR2_ARCH_GICv4,
		    ("gic_v3_read_ivar: Invalid GIC architecture: %d (%.08X)",
		     GICR_PIDR2_ARCH(sc->gic_pidr2), sc->gic_pidr2));
		*result = GICR_PIDR2_ARCH(sc->gic_pidr2);
		return (0);
	case GIC_IVAR_BUS:
		KASSERT(sc->gic_bus != GIC_BUS_UNKNOWN,
		    ("gic_v3_read_ivar: Unknown bus type"));
		KASSERT(sc->gic_bus <= GIC_BUS_MAX,
		    ("gic_v3_read_ivar: Invalid bus type %u", sc->gic_bus));
		*result = sc->gic_bus;
		return (0);
	case GIC_IVAR_VGIC:
		di = device_get_ivars(child);
		if (di == NULL)
			return (EINVAL);
		*result = di->is_vgic;
		return (0);
	}

	return (ENOENT);
}

static int
gic_v3_write_ivar(device_t dev, device_t child, int which, uintptr_t value)
{
	switch(which) {
	case GICV3_IVAR_NIRQS:
	case GICV3_IVAR_REDIST:
	case GIC_IVAR_HW_REV:
	case GIC_IVAR_BUS:
		return (EINVAL);
	}

	return (ENOENT);
}

static struct resource *
gic_v3_alloc_resource(device_t bus, device_t child, int type, int *rid,
    rman_res_t start, rman_res_t end, rman_res_t count, u_int flags)
{
	struct gic_v3_softc *sc;
	struct resource_list_entry *rle;
	struct resource_list *rl;
	int j;

	/* We only allocate memory */
	if (type != SYS_RES_MEMORY)
		return (NULL);

	sc = device_get_softc(bus);

	if (RMAN_IS_DEFAULT_RANGE(start, end)) {
		rl = BUS_GET_RESOURCE_LIST(bus, child);
		if (rl == NULL)
			return (NULL);

		/* Find defaults for this rid */
		rle = resource_list_find(rl, type, *rid);
		if (rle == NULL)
			return (NULL);

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

int
arm_gic_v3_intr(void *arg)
{
	struct gic_v3_softc *sc = arg;
	struct gic_v3_irqsrc *gi;
	struct intr_pic *pic;
	uint64_t active_irq;
	struct trapframe *tf;

	pic = sc->gic_pic;

	while (1) {
		if (CPU_MATCH_ERRATA_CAVIUM_THUNDERX_1_1) {
			/*
			 * Hardware:		Cavium ThunderX
			 * Chip revision:	Pass 1.0 (early version)
			 *			Pass 1.1 (production)
			 * ERRATUM:		22978, 23154
			 */
			__asm __volatile(
			    "nop;nop;nop;nop;nop;nop;nop;nop;	\n"
			    "mrs %0, ICC_IAR1_EL1		\n"
			    "nop;nop;nop;nop;			\n"
			    "dsb sy				\n"
			    : "=&r" (active_irq));
		} else {
			active_irq = gic_icc_read(IAR1);
		}

		if (active_irq >= GIC_FIRST_LPI) {
			intr_child_irq_handler(pic, active_irq);
			continue;
		}

		if (__predict_false(active_irq >= sc->gic_nirqs))
			return (FILTER_HANDLED);

		tf = curthread->td_intr_frame;
		gi = &sc->gic_irqs[active_irq];
		if (active_irq <= GIC_LAST_SGI) {
			/* Call EOI for all IPI before dispatch. */
			gic_icc_write(EOIR1, (uint64_t)active_irq);
#ifdef SMP
			intr_ipi_dispatch(sgi_to_ipi[gi->gi_irq]);
#else
			device_printf(sc->dev, "SGI %ju on UP system detected\n",
			    (uintmax_t)(active_irq - GIC_FIRST_SGI));
#endif
		} else if (active_irq >= GIC_FIRST_PPI &&
		    active_irq <= GIC_LAST_SPI) {
			if (gi->gi_trig == INTR_TRIGGER_EDGE)
				gic_icc_write(EOIR1, gi->gi_irq);

			if (intr_isrc_dispatch(&gi->gi_isrc, tf) != 0) {
				if (gi->gi_trig != INTR_TRIGGER_EDGE)
					gic_icc_write(EOIR1, gi->gi_irq);
				gic_v3_disable_intr(sc->dev, &gi->gi_isrc);
				device_printf(sc->dev,
				    "Stray irq %lu disabled\n", active_irq);
			}
		}
	}
}

#ifdef FDT
static int
gic_map_fdt(device_t dev, u_int ncells, pcell_t *cells, u_int *irqp,
    enum intr_polarity *polp, enum intr_trigger *trigp)
{
	u_int irq;

	if (ncells < 3)
		return (EINVAL);

	/*
	 * The 1st cell is the interrupt type:
	 *	0 = SPI
	 *	1 = PPI
	 * The 2nd cell contains the interrupt number:
	 *	[0 - 987] for SPI
	 *	[0 -  15] for PPI
	 * The 3rd cell is the flags, encoded as follows:
	 *   bits[3:0] trigger type and level flags
	 *	1 = edge triggered
	 *      2 = edge triggered (PPI only)
	 *	4 = level-sensitive
	 *	8 = level-sensitive (PPI only)
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

	switch (cells[2] & FDT_INTR_MASK) {
	case FDT_INTR_EDGE_RISING:
		*trigp = INTR_TRIGGER_EDGE;
		*polp = INTR_POLARITY_HIGH;
		break;
	case FDT_INTR_EDGE_FALLING:
		*trigp = INTR_TRIGGER_EDGE;
		*polp = INTR_POLARITY_LOW;
		break;
	case FDT_INTR_LEVEL_HIGH:
		*trigp = INTR_TRIGGER_LEVEL;
		*polp = INTR_POLARITY_HIGH;
		break;
	case FDT_INTR_LEVEL_LOW:
		*trigp = INTR_TRIGGER_LEVEL;
		*polp = INTR_POLARITY_LOW;
		break;
	default:
		device_printf(dev, "unsupported trigger/polarity "
		    "configuration 0x%02x\n", cells[2]);
		return (EINVAL);
	}

	/* Check the interrupt is valid */
	if (irq >= GIC_FIRST_SPI && *polp != INTR_POLARITY_HIGH)
		return (EINVAL);

	*irqp = irq;
	return (0);
}
#endif

static int
gic_map_msi(device_t dev, struct intr_map_data_msi *msi_data, u_int *irqp,
    enum intr_polarity *polp, enum intr_trigger *trigp)
{
	struct gic_v3_irqsrc *gi;

	/* SPI-mapped MSI */
	gi = (struct gic_v3_irqsrc *)msi_data->isrc;
	if (gi == NULL)
		return (ENXIO);

	*irqp = gi->gi_irq;

	/* MSI/MSI-X interrupts are always edge triggered with high polarity */
	*polp = INTR_POLARITY_HIGH;
	*trigp = INTR_TRIGGER_EDGE;

	return (0);
}

static int
do_gic_v3_map_intr(device_t dev, struct intr_map_data *data, u_int *irqp,
    enum intr_polarity *polp, enum intr_trigger *trigp)
{
	struct gic_v3_softc *sc;
	enum intr_polarity pol;
	enum intr_trigger trig;
	struct intr_map_data_msi *dam;
#ifdef FDT
	struct intr_map_data_fdt *daf;
#endif
#ifdef DEV_ACPI
	struct intr_map_data_acpi *daa;
#endif
	u_int irq;

	sc = device_get_softc(dev);

	switch (data->type) {
#ifdef FDT
	case INTR_MAP_DATA_FDT:
		daf = (struct intr_map_data_fdt *)data;
		if (gic_map_fdt(dev, daf->ncells, daf->cells, &irq, &pol,
		    &trig) != 0)
			return (EINVAL);
		break;
#endif
#ifdef DEV_ACPI
	case INTR_MAP_DATA_ACPI:
		daa = (struct intr_map_data_acpi *)data;
		irq = daa->irq;
		pol = daa->pol;
		trig = daa->trig;
		break;
#endif
	case INTR_MAP_DATA_MSI:
		/* SPI-mapped MSI */
		dam = (struct intr_map_data_msi *)data;
		if (gic_map_msi(dev, dam, &irq, &pol, &trig) != 0)
			return (EINVAL);
		break;
	default:
		return (EINVAL);
	}

	if (irq >= sc->gic_nirqs)
		return (EINVAL);
	switch (pol) {
	case INTR_POLARITY_CONFORM:
	case INTR_POLARITY_LOW:
	case INTR_POLARITY_HIGH:
		break;
	default:
		return (EINVAL);
	}
	switch (trig) {
	case INTR_TRIGGER_CONFORM:
	case INTR_TRIGGER_EDGE:
	case INTR_TRIGGER_LEVEL:
		break;
	default:
		return (EINVAL);
	}

	*irqp = irq;
	if (polp != NULL)
		*polp = pol;
	if (trigp != NULL)
		*trigp = trig;
	return (0);
}

static int
gic_v3_map_intr(device_t dev, struct intr_map_data *data,
    struct intr_irqsrc **isrcp)
{
	struct gic_v3_softc *sc;
	int error;
	u_int irq;

	error = do_gic_v3_map_intr(dev, data, &irq, NULL, NULL);
	if (error == 0) {
		sc = device_get_softc(dev);
		*isrcp = GIC_INTR_ISRC(sc, irq);
	}
	return (error);
}

struct gic_v3_setup_periph_args {
	device_t		 dev;
	struct intr_irqsrc	*isrc;
};

static void
gic_v3_setup_intr_periph(void *argp)
{
	struct gic_v3_setup_periph_args *args = argp;
	struct intr_irqsrc *isrc = args->isrc;
	struct gic_v3_irqsrc *gi = (struct gic_v3_irqsrc *)isrc;
	device_t dev = args->dev;
	u_int irq = gi->gi_irq;
	struct gic_v3_softc *sc = device_get_softc(dev);
	uint32_t reg;

	MPASS(irq <= GIC_LAST_SPI);

	/*
	 * We need the lock for both SGIs and PPIs for an atomic CPU_SET() at a
	 * minimum, but we also need it below for SPIs.
	 */
	mtx_lock_spin(&sc->gic_mtx);

	if (isrc->isrc_flags & INTR_ISRCF_PPI)
		CPU_SET(PCPU_GET(cpuid), &isrc->isrc_cpu);

	if (irq >= GIC_FIRST_PPI && irq <= GIC_LAST_SPI) {
		/* Set the trigger and polarity */
		if (irq <= GIC_LAST_PPI)
			reg = gic_r_read(sc, 4,
			    GICR_SGI_BASE_SIZE + GICD_ICFGR(irq));
		else
			reg = gic_d_read(sc, 4, GICD_ICFGR(irq));
		if (gi->gi_trig == INTR_TRIGGER_LEVEL)
			reg &= ~(2 << ((irq % 16) * 2));
		else
			reg |= 2 << ((irq % 16) * 2);

		if (irq <= GIC_LAST_PPI) {
			gic_r_write(sc, 4,
			    GICR_SGI_BASE_SIZE + GICD_ICFGR(irq), reg);
			gic_v3_wait_for_rwp(sc, REDIST);
		} else {
			gic_d_write(sc, 4, GICD_ICFGR(irq), reg);
			gic_v3_wait_for_rwp(sc, DIST);
		}
	}

	mtx_unlock_spin(&sc->gic_mtx);
}

static int
gic_v3_setup_intr(device_t dev, struct intr_irqsrc *isrc,
    struct resource *res, struct intr_map_data *data)
{
	struct gic_v3_irqsrc *gi = (struct gic_v3_irqsrc *)isrc;
	struct gic_v3_setup_periph_args pargs;
	enum intr_trigger trig;
	enum intr_polarity pol;
	u_int irq;
	int error;

	if (data == NULL)
		return (ENOTSUP);

	error = do_gic_v3_map_intr(dev, data, &irq, &pol, &trig);
	if (error != 0)
		return (error);

	if (gi->gi_irq != irq || pol == INTR_POLARITY_CONFORM ||
	    trig == INTR_TRIGGER_CONFORM)
		return (EINVAL);

	/* Compare config if this is not first setup. */
	if (isrc->isrc_handlers != 0) {
		if (pol != gi->gi_pol || trig != gi->gi_trig)
			return (EINVAL);
		else
			return (0);
	}

	/* For MSI/MSI-X we should have already configured these */
	if ((gi->gi_flags & GI_FLAG_MSI) == 0) {
		gi->gi_pol = pol;
		gi->gi_trig = trig;
	}

	pargs.dev = dev;
	pargs.isrc = isrc;

	if (isrc->isrc_flags & INTR_ISRCF_PPI) {
		/*
		 * If APs haven't been fired up yet, smp_rendezvous() will just
		 * execute it on the single CPU and gic_v3_init_secondary() will
		 * clean up afterwards.
		 */
		smp_rendezvous(NULL, gic_v3_setup_intr_periph, NULL, &pargs);
	} else if (irq >= GIC_FIRST_SPI && irq <= GIC_LAST_SPI) {
		gic_v3_setup_intr_periph(&pargs);
		gic_v3_bind_intr(dev, isrc);
	}

	return (0);
}

static int
gic_v3_teardown_intr(device_t dev, struct intr_irqsrc *isrc,
    struct resource *res, struct intr_map_data *data)
{
	struct gic_v3_irqsrc *gi = (struct gic_v3_irqsrc *)isrc;

	if (isrc->isrc_handlers == 0 && (gi->gi_flags & GI_FLAG_MSI) == 0) {
		gi->gi_pol = INTR_POLARITY_CONFORM;
		gi->gi_trig = INTR_TRIGGER_CONFORM;
	}

	return (0);
}

static void
gic_v3_disable_intr(device_t dev, struct intr_irqsrc *isrc)
{
	struct gic_v3_softc *sc;
	struct gic_v3_irqsrc *gi;
	u_int irq;

	sc = device_get_softc(dev);
	gi = (struct gic_v3_irqsrc *)isrc;
	irq = gi->gi_irq;

	if (irq <= GIC_LAST_PPI) {
		/* SGIs and PPIs in corresponding Re-Distributor */
		gic_r_write(sc, 4, GICR_SGI_BASE_SIZE + GICD_ICENABLER(irq),
		    GICD_I_MASK(irq));
		gic_v3_wait_for_rwp(sc, REDIST);
	} else if (irq >= GIC_FIRST_SPI && irq <= GIC_LAST_SPI) {
		/* SPIs in distributor */
		gic_d_write(sc, 4, GICD_ICENABLER(irq), GICD_I_MASK(irq));
		gic_v3_wait_for_rwp(sc, DIST);
	} else
		panic("%s: Unsupported IRQ %u", __func__, irq);
}

static void
gic_v3_enable_intr_periph(void *argp)
{
	struct gic_v3_setup_periph_args *args = argp;
	struct gic_v3_irqsrc *gi = (struct gic_v3_irqsrc *)args->isrc;
	device_t dev = args->dev;
	struct gic_v3_softc *sc = device_get_softc(dev);
	u_int irq = gi->gi_irq;

	/* SGIs and PPIs in corresponding Re-Distributor */
	gic_r_write(sc, 4, GICR_SGI_BASE_SIZE + GICD_ISENABLER(irq),
	    GICD_I_MASK(irq));
	gic_v3_wait_for_rwp(sc, REDIST);
}

static void
gic_v3_enable_intr(device_t dev, struct intr_irqsrc *isrc)
{
	struct gic_v3_setup_periph_args pargs;
	struct gic_v3_softc *sc;
	struct gic_v3_irqsrc *gi;
	u_int irq;

	gi = (struct gic_v3_irqsrc *)isrc;
	irq = gi->gi_irq;
	pargs.isrc = isrc;
	pargs.dev = dev;

	if (irq <= GIC_LAST_PPI) {
		/*
		 * SGIs only need configured on the current AP.  We'll setup and
		 * enable IPIs as APs come online.
		 */
		if (irq <= GIC_LAST_SGI)
			gic_v3_enable_intr_periph(&pargs);
		else
			smp_rendezvous(NULL, gic_v3_enable_intr_periph, NULL,
			    &pargs);
		return;
	}

	sc = device_get_softc(dev);

	if (irq >= GIC_FIRST_SPI && irq <= GIC_LAST_SPI) {
		/* SPIs in distributor */
		gic_d_write(sc, 4, GICD_ISENABLER(irq), GICD_I_MASK(irq));
		gic_v3_wait_for_rwp(sc, DIST);
	} else
		panic("%s: Unsupported IRQ %u", __func__, irq);
}

static void
gic_v3_pre_ithread(device_t dev, struct intr_irqsrc *isrc)
{
	struct gic_v3_irqsrc *gi = (struct gic_v3_irqsrc *)isrc;

	gic_v3_disable_intr(dev, isrc);
	gic_icc_write(EOIR1, gi->gi_irq);
}

static void
gic_v3_post_ithread(device_t dev, struct intr_irqsrc *isrc)
{

	gic_v3_enable_intr(dev, isrc);
}

static void
gic_v3_post_filter(device_t dev, struct intr_irqsrc *isrc)
{
	struct gic_v3_irqsrc *gi = (struct gic_v3_irqsrc *)isrc;

	if (gi->gi_trig == INTR_TRIGGER_EDGE)
		return;

	gic_icc_write(EOIR1, gi->gi_irq);
}

static int
gic_v3_bind_intr(device_t dev, struct intr_irqsrc *isrc)
{
	struct gic_v3_softc *sc;
	struct gic_v3_irqsrc *gi;
	int cpu;

	gi = (struct gic_v3_irqsrc *)isrc;

	KASSERT(gi->gi_irq >= GIC_FIRST_SPI && gi->gi_irq <= GIC_LAST_SPI,
	    ("%s: Attempting to bind an invalid IRQ", __func__));

	sc = device_get_softc(dev);

	if (CPU_EMPTY(&isrc->isrc_cpu)) {
		gic_irq_cpu = intr_irq_next_cpu(gic_irq_cpu, &all_cpus);
		CPU_SETOF(gic_irq_cpu, &isrc->isrc_cpu);
		gic_d_write(sc, 8, GICD_IROUTER(gi->gi_irq),
		    CPU_AFFINITY(gic_irq_cpu));
	} else {
		/*
		 * We can only bind to a single CPU so select
		 * the first CPU found.
		 */
		cpu = CPU_FFS(&isrc->isrc_cpu) - 1;
		gic_d_write(sc, 8, GICD_IROUTER(gi->gi_irq), CPU_AFFINITY(cpu));
	}

	return (0);
}

#ifdef SMP
static void
gic_v3_init_secondary(device_t dev, uint32_t rootnum)
{
	struct gic_v3_setup_periph_args pargs;
	device_t child;
	struct gic_v3_softc *sc;
	gic_v3_initseq_t *init_func;
	struct intr_irqsrc *isrc;
	u_int cpu, irq;
	int err, i;

	sc = device_get_softc(dev);
	cpu = PCPU_GET(cpuid);

	/* Train init sequence for boot CPU */
	for (init_func = gic_v3_secondary_init; *init_func != NULL;
	    init_func++) {
		err = (*init_func)(sc);
		if (err != 0) {
			device_printf(dev,
			    "Could not initialize GIC for CPU%u\n", cpu);
			return;
		}
	}

	pargs.dev = dev;

	/* Unmask attached SGI interrupts. */
	for (irq = GIC_FIRST_SGI; irq <= GIC_LAST_SGI; irq++) {
		isrc = GIC_INTR_ISRC(sc, irq);
		if (intr_isrc_init_on_cpu(isrc, cpu)) {
			pargs.isrc = isrc;
			gic_v3_enable_intr_periph(&pargs);
		}
	}

	/* Unmask attached PPI interrupts. */
	for (irq = GIC_FIRST_PPI; irq <= GIC_LAST_PPI; irq++) {
		isrc = GIC_INTR_ISRC(sc, irq);
		if (intr_isrc_init_on_cpu(isrc, cpu)) {
			pargs.isrc = isrc;
			gic_v3_setup_intr_periph(&pargs);
			gic_v3_enable_intr_periph(&pargs);
		}
	}

	for (i = 0; i < sc->gic_nchildren; i++) {
		child = sc->gic_children[i];
		PIC_INIT_SECONDARY(child, rootnum);
	}
}

static void
gic_v3_ipi_send(device_t dev, struct intr_irqsrc *isrc, cpuset_t cpus,
    u_int ipi)
{
	struct gic_v3_irqsrc *gi = (struct gic_v3_irqsrc *)isrc;
	uint64_t aff, val, irq;
	int i;

#define	GIC_AFF_MASK	(CPU_AFF3_MASK | CPU_AFF2_MASK | CPU_AFF1_MASK)
#define	GIC_AFFINITY(i)	(CPU_AFFINITY(i) & GIC_AFF_MASK)
	aff = GIC_AFFINITY(0);
	irq = gi->gi_irq;
	val = 0;

	/* Iterate through all CPUs in set */
	for (i = 0; i <= mp_maxid; i++) {
		/* Move to the next affinity group */
		if (aff != GIC_AFFINITY(i)) {
			/* Send the IPI */
			if (val != 0) {
				gic_icc_write(SGI1R, val);
				val = 0;
			}
			aff = GIC_AFFINITY(i);
		}

		/* Send the IPI to this cpu */
		if (CPU_ISSET(i, &cpus)) {
#define	ICC_SGI1R_AFFINITY(aff)					\
    (((uint64_t)CPU_AFF3(aff) << ICC_SGI1R_EL1_AFF3_SHIFT) |	\
     ((uint64_t)CPU_AFF2(aff) << ICC_SGI1R_EL1_AFF2_SHIFT) |	\
     ((uint64_t)CPU_AFF1(aff) << ICC_SGI1R_EL1_AFF1_SHIFT))
			/* Set the affinity when the first at this level */
			if (val == 0)
				val = ICC_SGI1R_AFFINITY(aff) |
				    irq << ICC_SGI1R_EL1_SGIID_SHIFT;
			/* Set the bit to send the IPI to te CPU */
			val |= 1 << CPU_AFF0(CPU_AFFINITY(i));
		}
	}

	/* Send the IPI to the last cpu affinity group */
	if (val != 0)
		gic_icc_write(SGI1R, val);
#undef GIC_AFF_MASK
#undef GIC_AFFINITY
}

static int
gic_v3_ipi_setup(device_t dev, u_int ipi, struct intr_irqsrc **isrcp)
{
	struct intr_irqsrc *isrc;
	struct gic_v3_softc *sc = device_get_softc(dev);

	if (sgi_first_unused > GIC_LAST_SGI)
		return (ENOSPC);

	isrc = GIC_INTR_ISRC(sc, sgi_first_unused);
	sgi_to_ipi[sgi_first_unused++] = ipi;

	CPU_SET(PCPU_GET(cpuid), &isrc->isrc_cpu);

	*isrcp = isrc;
	return (0);
}
#endif /* SMP */

/*
 * Helper routines
 */
static void
gic_v3_wait_for_rwp(struct gic_v3_softc *sc, enum gic_v3_xdist xdist)
{
	struct resource *res;
	bus_size_t offset;
	u_int cpuid;
	size_t us_left = 1000000;

	cpuid = PCPU_GET(cpuid);

	switch (xdist) {
	case DIST:
		res = sc->gic_dist;
		offset = 0;
		break;
	case REDIST:
		res = sc->gic_redists.pcpu[cpuid].res;
		offset = sc->gic_redists.pcpu[PCPU_GET(cpuid)].offset;
		break;
	default:
		KASSERT(0, ("%s: Attempt to wait for unknown RWP", __func__));
		return;
	}

	while ((bus_read_4(res, offset + GICD_CTLR) & GICD_CTLR_RWP) != 0) {
		DELAY(1);
		if (us_left-- == 0)
			panic("GICD Register write pending for too long");
	}
}

/* CPU interface. */
static __inline void
gic_v3_cpu_priority(uint64_t mask)
{

	/* Set prority mask */
	gic_icc_write(PMR, mask & ICC_PMR_EL1_PRIO_MASK);
}

static int
gic_v3_cpu_enable_sre(struct gic_v3_softc *sc)
{
	uint64_t sre;
	u_int cpuid;

	cpuid = PCPU_GET(cpuid);
	/*
	 * Set the SRE bit to enable access to GIC CPU interface
	 * via system registers.
	 */
	sre = READ_SPECIALREG(icc_sre_el1);
	sre |= ICC_SRE_EL1_SRE;
	WRITE_SPECIALREG(icc_sre_el1, sre);
	isb();
	/*
	 * Now ensure that the bit is set.
	 */
	sre = READ_SPECIALREG(icc_sre_el1);
	if ((sre & ICC_SRE_EL1_SRE) == 0) {
		/* We are done. This was disabled in EL2 */
		device_printf(sc->dev, "ERROR: CPU%u cannot enable CPU interface "
		    "via system registers\n", cpuid);
		return (ENXIO);
	} else if (bootverbose) {
		device_printf(sc->dev,
		    "CPU%u enabled CPU interface via system registers\n",
		    cpuid);
	}

	return (0);
}

static int
gic_v3_cpu_init(struct gic_v3_softc *sc)
{
	int err;

	/* Enable access to CPU interface via system registers */
	err = gic_v3_cpu_enable_sre(sc);
	if (err != 0)
		return (err);
	/* Priority mask to minimum - accept all interrupts */
	gic_v3_cpu_priority(GIC_PRIORITY_MIN);
	/* Disable EOI mode */
	gic_icc_clear(CTLR, ICC_CTLR_EL1_EOIMODE);
	/* Enable group 1 (insecure) interrups */
	gic_icc_set(IGRPEN1, ICC_IGRPEN0_EL1_EN);

	return (0);
}

/* Distributor */
static int
gic_v3_dist_init(struct gic_v3_softc *sc)
{
	uint64_t aff;
	u_int i;

	/*
	 * 1. Disable the Distributor
	 */
	gic_d_write(sc, 4, GICD_CTLR, 0);
	gic_v3_wait_for_rwp(sc, DIST);

	/*
	 * 2. Configure the Distributor
	 */
	/* Set all SPIs to be Group 1 Non-secure */
	for (i = GIC_FIRST_SPI; i < sc->gic_nirqs; i += GICD_I_PER_IGROUPRn)
		gic_d_write(sc, 4, GICD_IGROUPR(i), 0xFFFFFFFF);

	/* Set all global interrupts to be level triggered, active low. */
	for (i = GIC_FIRST_SPI; i < sc->gic_nirqs; i += GICD_I_PER_ICFGRn)
		gic_d_write(sc, 4, GICD_ICFGR(i), 0x00000000);

	/* Set priority to all shared interrupts */
	for (i = GIC_FIRST_SPI;
	    i < sc->gic_nirqs; i += GICD_I_PER_IPRIORITYn) {
		/* Set highest priority */
		gic_d_write(sc, 4, GICD_IPRIORITYR(i), GIC_PRIORITY_MAX);
	}

	/*
	 * Disable all interrupts. Leave PPI and SGIs as they are enabled in
	 * Re-Distributor registers.
	 */
	for (i = GIC_FIRST_SPI; i < sc->gic_nirqs; i += GICD_I_PER_ISENABLERn)
		gic_d_write(sc, 4, GICD_ICENABLER(i), 0xFFFFFFFF);

	gic_v3_wait_for_rwp(sc, DIST);

	/*
	 * 3. Enable Distributor
	 */
	/* Enable Distributor with ARE, Group 1 */
	gic_d_write(sc, 4, GICD_CTLR, GICD_CTLR_ARE_NS | GICD_CTLR_G1A |
	    GICD_CTLR_G1);

	/*
	 * 4. Route all interrupts to boot CPU.
	 */
	aff = CPU_AFFINITY(0);
	for (i = GIC_FIRST_SPI; i < sc->gic_nirqs; i++)
		gic_d_write(sc, 8, GICD_IROUTER(i), aff);

	return (0);
}

/* Re-Distributor */
static int
gic_v3_redist_alloc(struct gic_v3_softc *sc)
{
	sc->gic_redists.pcpu = mallocarray(mp_maxid + 1,
	    sizeof(sc->gic_redists.pcpu[0]), M_GIC_V3, M_WAITOK);
	return (0);
}

static int
gic_v3_redist_find(struct gic_v3_softc *sc)
{
	struct resource *r_res;
	bus_size_t offset;
	uint64_t aff;
	uint64_t typer;
	uint32_t pidr2;
	u_int cpuid;
	size_t i;

	cpuid = PCPU_GET(cpuid);

	aff = CPU_AFFINITY(cpuid);
	/* Affinity in format for comparison with typer */
	aff = (CPU_AFF3(aff) << 24) | (CPU_AFF2(aff) << 16) |
	    (CPU_AFF1(aff) << 8) | CPU_AFF0(aff);

	if (bootverbose) {
		device_printf(sc->dev,
		    "Start searching for Re-Distributor\n");
	}
	/* Iterate through Re-Distributor regions */
	for (i = 0; i < sc->gic_redists.nregions; i++) {
		/* Take a copy of the region's resource */
		r_res = sc->gic_redists.regions[i];

		pidr2 = bus_read_4(r_res, GICR_PIDR2);
		switch (GICR_PIDR2_ARCH(pidr2)) {
		case GICR_PIDR2_ARCH_GICv3: /* fall through */
		case GICR_PIDR2_ARCH_GICv4:
			break;
		default:
			device_printf(sc->dev,
			    "No Re-Distributor found for CPU%u\n", cpuid);
			return (ENODEV);
		}

		offset = 0;
		do {
			typer = bus_read_8(r_res, offset + GICR_TYPER);
			if ((typer >> GICR_TYPER_AFF_SHIFT) == aff) {
				KASSERT(cpuid <= mp_maxid,
				    ("Invalid pointer to per-CPU redistributor"));
				/* Copy res contents to its final destination */
				sc->gic_redists.pcpu[cpuid].res = r_res;
				sc->gic_redists.pcpu[cpuid].offset = offset;
				sc->gic_redists.pcpu[cpuid].lpi_enabled = false;
				if (bootverbose) {
					device_printf(sc->dev,
					    "CPU%u Re-Distributor has been found\n",
					    cpuid);
				}
				return (0);
			}

			offset += (GICR_RD_BASE_SIZE + GICR_SGI_BASE_SIZE);
			if ((typer & GICR_TYPER_VLPIS) != 0) {
				offset +=
				    (GICR_VLPI_BASE_SIZE + GICR_RESERVED_SIZE);
			}
		} while (offset < rman_get_size(r_res) &&
		    !sc->gic_redists.single && (typer & GICR_TYPER_LAST) == 0);
	}

	device_printf(sc->dev, "No Re-Distributor found for CPU%u\n", cpuid);
	return (ENXIO);
}

static int
gic_v3_redist_wake(struct gic_v3_softc *sc)
{
	uint32_t waker;
	size_t us_left = 1000000;

	waker = gic_r_read(sc, 4, GICR_WAKER);
	/* Wake up Re-Distributor for this CPU */
	waker &= ~GICR_WAKER_PS;
	gic_r_write(sc, 4, GICR_WAKER, waker);
	/*
	 * When clearing ProcessorSleep bit it is required to wait for
	 * ChildrenAsleep to become zero following the processor power-on.
	 */
	while ((gic_r_read(sc, 4, GICR_WAKER) & GICR_WAKER_CA) != 0) {
		DELAY(1);
		if (us_left-- == 0) {
			panic("Could not wake Re-Distributor for CPU%u",
			    PCPU_GET(cpuid));
		}
	}

	if (bootverbose) {
		device_printf(sc->dev, "CPU%u Re-Distributor woke up\n",
		    PCPU_GET(cpuid));
	}

	return (0);
}

static int
gic_v3_redist_init(struct gic_v3_softc *sc)
{
	int err;
	size_t i;

	err = gic_v3_redist_find(sc);
	if (err != 0)
		return (err);

	err = gic_v3_redist_wake(sc);
	if (err != 0)
		return (err);

	/* Configure SGIs and PPIs to be Group1 Non-secure */
	gic_r_write(sc, 4, GICR_SGI_BASE_SIZE + GICR_IGROUPR0,
	    0xFFFFFFFF);

	/* Disable SPIs */
	gic_r_write(sc, 4, GICR_SGI_BASE_SIZE + GICR_ICENABLER0,
	    GICR_I_ENABLER_PPI_MASK);
	/* Enable SGIs */
	gic_r_write(sc, 4, GICR_SGI_BASE_SIZE + GICR_ISENABLER0,
	    GICR_I_ENABLER_SGI_MASK);

	/* Set priority for SGIs and PPIs */
	for (i = 0; i <= GIC_LAST_PPI; i += GICR_I_PER_IPRIORITYn) {
		gic_r_write(sc, 4, GICR_SGI_BASE_SIZE + GICD_IPRIORITYR(i),
		    GIC_PRIORITY_MAX);
	}

	gic_v3_wait_for_rwp(sc, REDIST);

	return (0);
}

/*
 * SPI-mapped Message Based Interrupts -- a GICv3 MSI/MSI-X controller.
 */

static int
gic_v3_gic_alloc_msi(device_t dev, u_int mbi_start, u_int mbi_count,
    int count, int maxcount, struct intr_irqsrc **isrc)
{
	struct gic_v3_softc *sc;
	int i, irq, end_irq;
	bool found;

	KASSERT(powerof2(count), ("%s: bad count", __func__));
	KASSERT(powerof2(maxcount), ("%s: bad maxcount", __func__));

	sc = device_get_softc(dev);

	mtx_lock(&sc->gic_mbi_mtx);

	found = false;
	for (irq = mbi_start; irq < mbi_start + mbi_count; irq++) {
		/* Start on an aligned interrupt */
		if ((irq & (maxcount - 1)) != 0)
			continue;

		/* Assume we found a valid range until shown otherwise */
		found = true;

		/* Check this range is valid */
		for (end_irq = irq; end_irq != irq + count; end_irq++) {
			/* No free interrupts */
			if (end_irq == mbi_start + mbi_count) {
				found = false;
				break;
			}

			KASSERT((sc->gic_irqs[end_irq].gi_flags & GI_FLAG_MSI)!= 0,
			    ("%s: Non-MSI interrupt found", __func__));

			/* This is already used */
			if ((sc->gic_irqs[end_irq].gi_flags & GI_FLAG_MSI_USED) ==
			    GI_FLAG_MSI_USED) {
				found = false;
				break;
			}
		}
		if (found)
			break;
	}

	/* Not enough interrupts were found */
	if (!found || irq == mbi_start + mbi_count) {
		mtx_unlock(&sc->gic_mbi_mtx);
		return (ENXIO);
	}

	for (i = 0; i < count; i++) {
		/* Mark the interrupt as used */
		sc->gic_irqs[irq + i].gi_flags |= GI_FLAG_MSI_USED;
	}
	mtx_unlock(&sc->gic_mbi_mtx);

	for (i = 0; i < count; i++)
		isrc[i] = (struct intr_irqsrc *)&sc->gic_irqs[irq + i];

	return (0);
}

static int
gic_v3_gic_release_msi(device_t dev, int count, struct intr_irqsrc **isrc)
{
	struct gic_v3_softc *sc;
	struct gic_v3_irqsrc *gi;
	int i;

	sc = device_get_softc(dev);

	mtx_lock(&sc->gic_mbi_mtx);
	for (i = 0; i < count; i++) {
		gi = (struct gic_v3_irqsrc *)isrc[i];

		KASSERT((gi->gi_flags & GI_FLAG_MSI_USED) == GI_FLAG_MSI_USED,
		    ("%s: Trying to release an unused MSI-X interrupt",
		    __func__));

		gi->gi_flags &= ~GI_FLAG_MSI_USED;
	}
	mtx_unlock(&sc->gic_mbi_mtx);

	return (0);
}

static int
gic_v3_gic_alloc_msix(device_t dev, u_int mbi_start, u_int mbi_count,
    struct intr_irqsrc **isrcp)
{
	struct gic_v3_softc *sc;
	int irq;

	sc = device_get_softc(dev);

	mtx_lock(&sc->gic_mbi_mtx);
	/* Find an unused interrupt */
	for (irq = mbi_start; irq < mbi_start + mbi_count; irq++) {
		KASSERT((sc->gic_irqs[irq].gi_flags & GI_FLAG_MSI) != 0,
		    ("%s: Non-MSI interrupt found", __func__));
		if ((sc->gic_irqs[irq].gi_flags & GI_FLAG_MSI_USED) == 0)
			break;
	}
	/* No free interrupt was found */
	if (irq == mbi_start + mbi_count) {
		mtx_unlock(&sc->gic_mbi_mtx);
		return (ENXIO);
	}

	/* Mark the interrupt as used */
	sc->gic_irqs[irq].gi_flags |= GI_FLAG_MSI_USED;
	mtx_unlock(&sc->gic_mbi_mtx);

	*isrcp = (struct intr_irqsrc *)&sc->gic_irqs[irq];

	return (0);
}

static int
gic_v3_gic_release_msix(device_t dev, struct intr_irqsrc *isrc)
{
	struct gic_v3_softc *sc;
	struct gic_v3_irqsrc *gi;

	sc = device_get_softc(dev);
	gi = (struct gic_v3_irqsrc *)isrc;

	KASSERT((gi->gi_flags & GI_FLAG_MSI_USED) == GI_FLAG_MSI_USED,
	    ("%s: Trying to release an unused MSI-X interrupt", __func__));

	mtx_lock(&sc->gic_mbi_mtx);
	gi->gi_flags &= ~GI_FLAG_MSI_USED;
	mtx_unlock(&sc->gic_mbi_mtx);

	return (0);
}

static int
gic_v3_alloc_msi(device_t dev, device_t child, int count, int maxcount,
    device_t *pic, struct intr_irqsrc **isrc)
{
	struct gic_v3_softc *sc;
	int error;

	sc = device_get_softc(dev);
	error = gic_v3_gic_alloc_msi(dev, sc->gic_mbi_start,
	    sc->gic_mbi_end - sc->gic_mbi_start, count, maxcount, isrc);
	if (error != 0)
		return (error);

	*pic = dev;
	return (0);
}

static int
gic_v3_release_msi(device_t dev, device_t child, int count,
    struct intr_irqsrc **isrc)
{
	return (gic_v3_gic_release_msi(dev, count, isrc));
}

static int
gic_v3_alloc_msix(device_t dev, device_t child, device_t *pic,
    struct intr_irqsrc **isrc)
{
	struct gic_v3_softc *sc;
	int error;

	sc = device_get_softc(dev);
	error = gic_v3_gic_alloc_msix(dev, sc->gic_mbi_start,
	    sc->gic_mbi_end - sc->gic_mbi_start, isrc);
	if (error != 0)
		return (error);

	*pic = dev;

	return (0);
}

static int
gic_v3_release_msix(device_t dev, device_t child, struct intr_irqsrc *isrc)
{
	return (gic_v3_gic_release_msix(dev, isrc));
}

static int
gic_v3_map_msi(device_t dev, device_t child, struct intr_irqsrc *isrc,
    uint64_t *addr, uint32_t *data)
{
	struct gic_v3_softc *sc = device_get_softc(dev);
	struct gic_v3_irqsrc *gi = (struct gic_v3_irqsrc *)isrc;

	*addr = vtophys(rman_get_virtual(sc->gic_dist)) + GICD_SETSPI_NSR;
	*data = gi->gi_irq;

	return (0);
}
